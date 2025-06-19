// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#include <asm/byteorder.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include "rsa.h"

#define DIGIT_T unsigned int
#define HALF_DIGIT_T unsigned short

#define MAX_DIGIT 0xffffffffUL
#define MAX_HALF_DIGIT 0xffffUL	/* NB 'L' */
#define BITS_PER_DIGIT 32
#define HIBITMASK 0x80000000UL
#define B (MAX_HALF_DIGIT + 1)

#define BITS_PER_HALF_DIGIT (BITS_PER_DIGIT / 2)
#define BYTES_PER_DIGIT (BITS_PER_DIGIT / 8)

/* Useful macros */
#define LOHALF(x) ((DIGIT_T)((x) & MAX_HALF_DIGIT))
#define HIHALF(x) ((DIGIT_T)((x) >> BITS_PER_HALF_DIGIT & MAX_HALF_DIGIT))
#define TOHIGH(x) ((DIGIT_T)((x) << BITS_PER_HALF_DIGIT))

#define E_LEN 4

#define STORE32H(x, y)                                                                  \
	{ (y)[0] = (unsigned char)(((x)>>24)&255); (y)[1] = (unsigned char)(((x)>>16)&255); \
	(y)[2] = (unsigned char)(((x)>>8)&255); (y)[3] = (unsigned char)((x)&255); }

/**
 * Perform LTC_PKCS #1 MGF1 (internal)
 * @param seed        The seed for MGF1
 * @param seedlen     The length of the seed
 * @param mask        [out] The destination
 * @param masklen     The length of the mask desired
 *@return 0 if successful
 */
int pkcs_1_mgf1_sha256(const unsigned char *seed, unsigned long seedlen,
		unsigned char *mask, unsigned long masklen)
{
	unsigned long hLen, x;
	unsigned int  counter;
	int              err;
	unsigned char buf[32];
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int desc_size = 0;

	/* get hash output size */
	hLen = 32;

	/* start counter */
	counter = 0;


	while (masklen > 0) {
		/* handle counter */
		STORE32H(counter, buf);
		++counter;

		/* get hash of seed || counter */
		/**/
		tfm = crypto_alloc_shash("sha256", 0, 0);
		desc_size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
		desc = kmalloc(desc_size, GFP_KERNEL);
		if (desc == NULL)
			return -1;

		desc->tfm = tfm;
		crypto_shash_init(desc);
		crypto_shash_update(desc, seed, seedlen);
		crypto_shash_update(desc, buf, 4);
		crypto_shash_final(desc, buf);

		/* store it */
		for (x = 0; x < hLen && masklen > 0; x++, masklen--)
			*mask++ = buf[x];

	}

	err = 0;

	return err;
}

/**
 * LTC_PKCS #1 v2.00 PSS decode, salt len 222, modulus_len 256
 * @param  msghash            The hash to verify
 * @param  msghashlen        The length of the hash (octets)
 * @param  sig                 The signature data (encoded data)
 * @param  siglen             The length of the signature data (octets)
 * @param  saltlen            The length of the salt used (octets)
 * @param  modulus_bitlen  The bit length of the RSA modulus
 * @param  res                 [out] The result of the comparison, 1==valid, 0==invalid
 * @return 0 if successful (even if the comparison failed)
 */
int pkcs_1_pss_decode_sha256(const unsigned char *msghash, unsigned long msghashlen,
			     unsigned char *sig, unsigned long siglen, unsigned long saltlen,
			     unsigned long modulus_bitlen, int *res)
{
	unsigned char *DB = kmalloc(1024, GFP_KERNEL);
	unsigned char *mask = kmalloc(1024, GFP_KERNEL);
	unsigned char *salt = kmalloc(1024, GFP_KERNEL);
	unsigned char *hash = kmalloc(1024, GFP_KERNEL);
	unsigned int hash_xor = 0;
	unsigned long x, y, hLen, modulus_len;
	int err;
	unsigned long i;
	*res = 0;

	hLen = 32;
	modulus_len = (modulus_bitlen>>3) + (modulus_bitlen & 7 ? 1 : 0);

	if (sig[siglen-1] != 0xBC) {
		err = -4;
		goto error;
	}
	if (DB)
		memset(DB, 0, 256);

	x = 0;
	memcpy((void *)DB, (void *)(sig + x), modulus_len - hLen - 1);
	x += modulus_len - hLen - 1;
	if (hash)
		memset(hash, 0, 256);

	for (i = 0; i < hLen; i++)
		hash[i] = sig[x + i];

	x += hLen;

	if ((sig[0] & ~(0xFF >> ((modulus_len<<3) - (modulus_bitlen-1)))) != 0) {
		err = -5;
		goto error;
	}
	if (mask) {
		err = pkcs_1_mgf1_sha256(hash, hLen, mask, modulus_len - hLen - 1);
		if (err != 0)
			goto error;
	}

	for (y = 0; y < (modulus_len - hLen - 1); y++)
		DB[y] ^= mask[y];

	DB[0] &= 0xFF >> ((modulus_len<<3) - (modulus_bitlen-1));

	for (x = 0; x < modulus_len - saltlen - hLen - 2; x++) {
		if (DB[x] != 0x00) {
			err = -6;
			goto error;
		}
	}

	if (DB[x++] != 0x01) {
		err = -7;
		goto error;
	}

	memcpy(mask, msghash, msghashlen);

	for (i = 0; i < hLen; i++)
		hash_xor += mask[i] ^ hash[i];
	if (hash_xor == 0)
		*res = 1;

	err = 0;

error:
	kfree(DB);
	kfree(mask);
	kfree(salt);
	kfree(hash);
	return err;
}

static int spMultiply(DIGIT_T p[2], DIGIT_T x, DIGIT_T y)
{
	*(uint64_t *) p = (uint64_t) x * y;
	return 0;
}

static inline int mpMultiply(DIGIT_T w[],
			     const DIGIT_T u[], const DIGIT_T v[],
			     unsigned int ndigits)
{
	/* Computes product w = u * v
	 * where u, v are multiprecision integers of ndigits each
	 * and w is a multiprecision integer of 2*ndigits
	 * Ref: Knuth Vol 2 Ch 4.3.1 p 268 Algorithm M.
	 */

	DIGIT_T k, t[2];
	unsigned int i, j, m, n;

	m = n = ndigits;

	/* Step M1. Initialise */
	for (i = 0; i < 2 * m; i++)
		w[i] = 0;

	for (j = 0; j < n; j++) {
		/* Step M2. Zero multiplier? */
		if (v[j] == 0) {
			w[j + m] = 0;
		} else {
			/* Step M3. Initialise i */
			k = 0;
			for (i = 0; i < m; i++) {
				/* Step M4. Multiply and add */
				/* t = u_i * v_j + w_(i+j) + k */
				spMultiply(t, u[i], v[j]);

				t[0] += k;
				if (t[0] < k)
					t[1]++;
				t[0] += w[i + j];
				if (t[0] < w[i + j])
					t[1]++;

				w[i + j] = t[0];
				k = t[1];
			}
			/* Step M5. Loop on i, set w_(j+m) = k */
			w[j + m] = k;
		}
	}	/* Step M6. Loop on j */

	return 0;
}

static void mpSetZero(DIGIT_T a[], size_t ndigits)
{
	/* Prevent optimiser ignoring this */
	DIGIT_T optdummy;
	DIGIT_T *p = a;

	while (ndigits--)
		a[ndigits] = 0;

	optdummy = *p;
}

static DIGIT_T mpShiftRight(DIGIT_T a[], const DIGIT_T b[],
		size_t shift, size_t ndigits)
{
	/* [v2.1] Modified to cope with shift > BITS_PERDIGIT */
	size_t i, y, nw, bits;
	volatile DIGIT_T mask, carry, nextcarry;

	/* Do we shift whole digits? */
	if (shift >= BITS_PER_DIGIT) {
		nw = shift / BITS_PER_DIGIT;
		for (i = 0; i < ndigits; i++) {
			if ((i + nw) < ndigits)
				a[i] = b[i + nw];
			else
				a[i] = 0;
		}
		/* Call again to shift bits inside digits */
		bits = shift % BITS_PER_DIGIT;
		carry = b[nw - 1] >> bits;
		if (bits)
			carry |= mpShiftRight(a, a, bits, ndigits);
		return carry;
	}

	bits = shift;
	/* Construct mask to set low bits */
	/* (thanks to Jesse Chisholm for suggesting this improved technique) */
	mask = ~(~(DIGIT_T) 0 << bits);

	y = BITS_PER_DIGIT - bits;
	carry = 0;
	i = ndigits;
	while (i--) {
		nextcarry = (b[i] & mask) << y;
		a[i] = b[i] >> bits | carry;
		carry = nextcarry;
	}

	return carry;
}

static size_t mpSizeof(const DIGIT_T a[], size_t ndigits)
{				/* Returns size of significant digits in a */

	while (ndigits--) {
		if (a[ndigits] != 0)
			return (++ndigits);
	}
	return 0;
}

static void mpSetEqual(DIGIT_T a[], const DIGIT_T b[], size_t ndigits)
{				/* Sets a = b */
	size_t i;

	for (i = 0; i < ndigits; i++)
		a[i] = b[i];
}

static DIGIT_T mpShiftLeft(DIGIT_T a[], const DIGIT_T *b,
	size_t shift, size_t ndigits)
{
	/* Computes a = b << shift */
	/* [v2.1] Modified to cope with shift > BITS_PERDIGIT */
	size_t i, y, nw, bits;
	volatile DIGIT_T mask, carry, nextcarry;

	/* Do we shift whole digits? */
	if (shift >= BITS_PER_DIGIT) {
		nw = shift / BITS_PER_DIGIT;
		i = ndigits;
		while (i--) {
			if (i >= nw)
				a[i] = b[i - nw];
			else
				a[i] = 0;
		}
		/* Call again to shift bits inside digits */
		bits = shift % BITS_PER_DIGIT;
		carry = b[ndigits - nw] << bits;
		if (bits)
			carry |= mpShiftLeft(a, a, bits, ndigits);
		return carry;
	}

	bits = shift;
	/* Construct mask = high bits set */
	mask = ~(~(DIGIT_T) 0 >> bits);

	y = BITS_PER_DIGIT - bits;
	carry = 0;
	for (i = 0; i < ndigits; i++) {
		nextcarry = (b[i] & mask) >> y;
		a[i] = b[i] << bits | carry;
		carry = nextcarry;
	}

	return carry;
}

static void spMultSub(DIGIT_T uu[2], DIGIT_T qhat, DIGIT_T v1, DIGIT_T v0)
{
	DIGIT_T p0, p1, t;

	p0 = qhat * v0;
	p1 = qhat * v1;
	t = p0 + TOHIGH(LOHALF(p1));
	uu[0] -= t;
	if (uu[0] > MAX_DIGIT - t)
		uu[1]--;	/* Borrow */
	uu[1] -= HIHALF(p1);
}

static DIGIT_T spDivide(DIGIT_T *q, DIGIT_T *r, const DIGIT_T u[2], DIGIT_T v)
{
	DIGIT_T qhat, rhat, t, v0, v1, u0, u1, u2, u3;
	DIGIT_T uu[2], q2;

	/* Check for normalisation */
	if (!(v & HIBITMASK)) {
		/* Stop if assert is working, else return error */
		/*assert(v & HIBITMASK); */
		*q = *r = 0;
		return MAX_DIGIT;
	}

	/* Split up into half-digits */
	v0 = LOHALF(v);
	v1 = HIHALF(v);
	u0 = LOHALF(u[0]);
	u1 = HIHALF(u[0]);
	u2 = LOHALF(u[1]);
	u3 = HIHALF(u[1]);

	qhat = (u3 < v1 ? 0 : 1);
	if (qhat > 0) {		/* qhat is one, so no need to mult */
		rhat = u3 - v1;
		/* t = r.b + u2 */
		t = TOHIGH(rhat) | u2;
		if (v0 > t)
			qhat--;
	}

	uu[1] = 0;		/* (u4) */
	uu[0] = u[1];		/* (u3u2) */
	if (qhat > 0) {
		/* (u4u3u2) -= qhat(v1v0) where u4 = 0 */
		spMultSub(uu, qhat, v1, v0);
		if (HIHALF(uu[1]) != 0) {	/* Add back */
			qhat--;
			uu[0] += v;
			uu[1] = 0;
		}
	}
	q2 = qhat;

	t = uu[0];
	qhat = t / v1;
	rhat = t - qhat * v1;
	/* Test on v0 */
	t = TOHIGH(rhat) | u1;
	if ((qhat == B) || (qhat * v0 > t)) {
		qhat--;
		rhat += v1;
		t = TOHIGH(rhat) | u1;
		if ((rhat < B) && (qhat * v0 > t))
			qhat--;
	}

	uu[1] = HIHALF(uu[0]);	/* (0u3) */
	uu[0] = TOHIGH(LOHALF(uu[0])) | u1;	/* (u2u1) */
	spMultSub(uu, qhat, v1, v0);
	if (HIHALF(uu[1]) != 0) {	/* Add back */
		qhat--;
		uu[0] += v;
		uu[1] = 0;
	}

	/* q1 = qhat */
	*q = TOHIGH(qhat);

	t = uu[0];
	qhat = t / v1;
	rhat = t - qhat * v1;
	/* Test on v0 */
	t = TOHIGH(rhat) | u0;
	if ((qhat == B) || (qhat * v0 > t)) {
		qhat--;
		rhat += v1;
		t = TOHIGH(rhat) | u0;
		if ((rhat < B) && (qhat * v0 > t))
			qhat--;
	}

	uu[1] = HIHALF(uu[0]);	/* (0u2) */
	uu[0] = TOHIGH(LOHALF(uu[0])) | u0;	/* (u1u0) */
	spMultSub(uu, qhat, v1, v0);
	if (HIHALF(uu[1]) != 0) {	/* Add back */
		qhat--;
		uu[0] += v;
		uu[1] = 0;
	}

	/* q0 = qhat */
	*q |= LOHALF(qhat);

	/* Remainder is in (u1u0) i.e. uu[0] */
	*r = uu[0];
	return q2;
}

static inline DIGIT_T mpShortDiv(DIGIT_T q[], const DIGIT_T u[],
	DIGIT_T v, size_t ndigits)
{
	size_t j;
	DIGIT_T t[2], r;
	size_t shift;
	DIGIT_T bitmask, overflow, *uu;

	if (ndigits == 0)
		return 0;
	if (v == 0)
		return 0;	/* Divide by zero error */

	bitmask = HIBITMASK;
	for (shift = 0; shift < BITS_PER_DIGIT; shift++) {
		if (v & bitmask)
			break;
		bitmask >>= 1;
	}

	v <<= shift;
	overflow = mpShiftLeft(q, u, shift, ndigits);
	uu = q;

	/* Step S1 - modified for extra digit. */
	r = overflow;		/* New digit Un */
	j = ndigits;
	while (j--) {
		/* Step S2. */
		t[1] = r;
		t[0] = uu[j];
		overflow = spDivide(&q[j], &r, t, v);
	}

	/* Unnormalise */
	r >>= shift;

	return r;
}


static inline int mpCompare(const DIGIT_T a[], const DIGIT_T b[],
	size_t ndigits)
{
	/* Returns sign of (a - b)
	 */

	if (ndigits == 0)
		return 0;

	while (ndigits--) {
		if (a[ndigits] > b[ndigits])
			return 1;	/* GT */
		if (a[ndigits] < b[ndigits])
			return -1;	/* LT */
	}

	return 0;		/* EQ */
}

static inline void mpSetDigit(DIGIT_T a[], DIGIT_T d, size_t ndigits)
{
	/* Sets a = d where d is a single digit */
	size_t i;

	for (i = 1; i < ndigits; i++)
		a[i] = 0;
	a[0] = d;
}

static int QhatTooBig(DIGIT_T qhat, DIGIT_T rhat, DIGIT_T vn2, DIGIT_T ujn2)
{
	DIGIT_T t[2];

	spMultiply(t, qhat, vn2);
	if (t[1] < rhat)
		return 0;
	else if (t[1] > rhat)
		return 1;
	else if (t[0] > ujn2)
		return 1;

	return 0;
}

static inline DIGIT_T mpMultSub(DIGIT_T wn, DIGIT_T w[],
	const DIGIT_T v[], DIGIT_T q, size_t n)
{
	DIGIT_T k, t[2];
	size_t i;

	if (q == 0)		/* No change */
		return wn;

	k = 0;

	for (i = 0; i < n; i++) {
		spMultiply(t, q, v[i]);
		w[i] -= k;
		if (w[i] > MAX_DIGIT - k)
			k = 1;
		else
			k = 0;
		w[i] -= t[0];
		if (w[i] > MAX_DIGIT - t[0])
			k++;
		k += t[1];
	}

	/* Cope with Wn not stored in array w[0..n-1] */
	wn -= k;

	return wn;
}

static inline DIGIT_T mpAdd(DIGIT_T w[], const DIGIT_T u[],
	const DIGIT_T v[], size_t ndigits)
{
	DIGIT_T k;
	size_t j;

	/*assert(w != v);*/
	k = 0;

	for (j = 0; j < ndigits; j++) {
		w[j] = u[j] + k;
		if (w[j] < k)
			k = 1;
		else
			k = 0;

		w[j] += v[j];
		if (w[j] < v[j])
			k++;

	}

	return k;
}

static inline int mpDivide(DIGIT_T q[], DIGIT_T r[], const DIGIT_T u[],
	unsigned int udigits, DIGIT_T v[],
	unsigned int vdigits)
{
	unsigned int shift;
	int n, m, j;
	DIGIT_T bitmask, overflow;
	DIGIT_T qhat, rhat, t[2];
	DIGIT_T *uu, *ww;
	int qhatOK, cmp;

	/* Clear q and r */
	mpSetZero(q, udigits);
	mpSetZero(r, udigits);

	/* Work out exact sizes of u and v */
	n = (int)mpSizeof(v, vdigits);
	m = (int)mpSizeof(u, udigits);
	m -= n;

	/* Catch special cases */
	if (n == 0)
		return -1;	/* Error: divide by zero */

	if (n == 1) {		/* Use short division instead */
		r[0] = mpShortDiv(q, u, v[0], udigits);
		return 0;
	}

	if (m < 0) {		/* v > u, so just set q = 0 and r = u */
		mpSetEqual(r, u, udigits);
		return 0;
	}

	if (m == 0) {		/* u and v are the same length */
		cmp = mpCompare(u, v, (size_t) n);
		if (cmp < 0) {	/* v > u, as above */
			mpSetEqual(r, u, udigits);
			return 0;
		} else if (cmp == 0) {	/* v == u, so set q = 1 and r = 0 */
			mpSetDigit(q, 1, udigits);
			return 0;
		}
	}

	bitmask = HIBITMASK;
	for (shift = 0; shift < BITS_PER_DIGIT; shift++) {
		if (v[n - 1] & bitmask)
			break;
		bitmask >>= 1;
	}

	/* Normalise v in situ - NB only shift non-zero digits */
	overflow = mpShiftLeft(v, v, shift, n);

	/* Copy normalised dividend u*d into r */
	overflow = mpShiftLeft(r, u, shift, n + m);
	uu = r;			/* Use ptr to keep notation constant */

	t[0] = overflow;	/* Extra digit Um+n */

	/* Step D2. Initialise j. Set j = m */
	for (j = m; j >= 0; j--) {
		/* Step D3. Set Qhat = [(b.Uj+n + Uj+n-1)/Vn-1]
		 * and Rhat = remainder
		 */
		qhatOK = 0;
		t[1] = t[0];	/* This is Uj+n */
		t[0] = uu[j + n - 1];
		overflow = spDivide(&qhat, &rhat, t, v[n - 1]);

		/* Test Qhat */
		if (overflow) {	/* Qhat == b so set Qhat = b - 1 */
			qhat = MAX_DIGIT;
			rhat = uu[j + n - 1];
			rhat += v[n - 1];
			if (rhat < v[n - 1])	/* Rhat >= b, so no re-test */
				qhatOK = 1;
		}
		/* [VERSION 2: Added extra test "qhat && "] */
		if (qhat &&
			!qhatOK &&
			QhatTooBig(qhat, rhat, v[n - 2], uu[j + n - 2])) {
			qhat--;
			rhat += v[n - 1];
			/* Repeat this test if Rhat < b */
			if (!(rhat < v[n - 1]))
				if (QhatTooBig(qhat, rhat,
						v[n - 2], uu[j + n - 2]))
					qhat--;
		}


		/* Step D4. Multiply and subtract */
		ww = &uu[j];
		overflow = mpMultSub(t[1], ww, v, qhat, (size_t) n);

		/* Step D5. Test remainder. Set Qj = Qhat */
		q[j] = qhat;
		if (overflow) {	/* Step D6. Add back if D4 was negative */
			q[j]--;
			overflow = mpAdd(ww, ww, v, (size_t) n);
		}

		t[0] = uu[j + n - 1];	/* Uj+n on next round */

	}			/* Step D7. Loop on j */

	/* Clear high digits in uu */
	for (j = n; j < m + n; j++)
		uu[j] = 0;

	/* Step D8. Unnormalise. */

	mpShiftRight(r, r, shift, n);
	mpShiftRight(v, v, shift, n);

	return 0;
}

static inline int moduloTemp(DIGIT_T r[], const DIGIT_T u[],
			size_t udigits, DIGIT_T v[], size_t vdigits,
			DIGIT_T tqq[], DIGIT_T trr[])
{
	mpDivide(tqq, trr, u, udigits, v, vdigits);

	/* Final r is only vdigits long */
	mpSetEqual(r, trr, vdigits);

	return 0;
}

static int modMultTemp(DIGIT_T a[],
		       const DIGIT_T x[],
		       const DIGIT_T y[],
		       DIGIT_T m[], size_t ndigits,
		       DIGIT_T temp[], DIGIT_T tqq[], DIGIT_T trr[])
{
	/*  Computes a = (x * y) mod m */
	/*  Requires 3 x temp mp's of length 2 * ndigits each */

	/* Calc p[2n] = x * y */
	mpMultiply(temp, x, y, ndigits);

	/* Then modulo m */
	moduloTemp(a, temp, ndigits * 2, m, ndigits, tqq, trr);

	return 0;
}

/*---------------------------------------------------------------------
 * Function      : RSA
 * Description  : Verify signature by RSA, called by fw integraty check
 * Parameter   :
 * pu1Signature    [in]: The signature must be 2048bit
 * pu4CheckSum  [in/out]: Checksum value must be 2048bit
 * Return      : verify OK or not
 */
static inline int RSADecryption65537(unsigned int *pu1Signature,
		unsigned int *pu4PublicKey, unsigned int *pu4CheckSum)
{
	int i = 0;
	DIGIT_T *y_2048 = NULL;
	DIGIT_T *t1_4096 = NULL;
	DIGIT_T *t2_4096 = NULL;
	DIGIT_T *t3_4096 = NULL;

	y_2048 = kmalloc_array(64, sizeof(DIGIT_T), GFP_KERNEL);
	if (y_2048 == NULL)
		return -1;
	t1_4096 = kmalloc_array(128, sizeof(DIGIT_T), GFP_KERNEL);
	if (t1_4096 == NULL) {
		kfree(y_2048);
		return -1;
	}
	t2_4096 = kmalloc_array(128, sizeof(DIGIT_T), GFP_KERNEL);
	if (t2_4096 == NULL) {
		kfree(y_2048);
		kfree(t1_4096);
		return -1;
	}
	t3_4096 = kmalloc_array(128, sizeof(DIGIT_T), GFP_KERNEL);
	if (t3_4096 == NULL) {
		kfree(y_2048);
		kfree(t1_4096);
		kfree(t2_4096);
		return -1;
	}

	/* Read the public key*/

	/*y = s * s mod n*/
	modMultTemp(y_2048, pu1Signature, pu1Signature,
			pu4PublicKey, 64, t1_4096, t2_4096,
		    t3_4096);
	/*(y = y * y mod n) ^ 15 */
	for (; i < 15; i++)
		modMultTemp(y_2048, y_2048, y_2048,
			pu4PublicKey, 64, t1_4096, t2_4096, t3_4096);
	/*y = y * s mode n*/
	modMultTemp(y_2048, y_2048, pu1Signature,
			pu4PublicKey, 64, t1_4096, t2_4096, t3_4096);

	memcpy(pu4CheckSum, y_2048, 256);

	kfree(y_2048);
	kfree(t1_4096);
	kfree(t2_4096);
	kfree(t3_4096);
	return 0;
}

uint8_t *rsa_encryptdecrypt(const uint8_t *sig,
	const uint8_t *e, const uint8_t *n)
{
	uint8_t *out_t;
	uint8_t *sig_t;
	uint8_t *out;
	uint8_t *key_tmp;
	int i = 0;

	uint8_t excepted_e[E_LEN] = {0x00, 0x01, 0x00, 0x01};

	if (memcmp(e, excepted_e, E_LEN))	{
		pr_notice("e value check fail,only support e value as 65537!\n");
		return NULL;
	}

	out_t = kmalloc(RSA_LEN, GFP_KERNEL);
	if (out_t == NULL)
		return NULL;
	sig_t = kmalloc(RSA_LEN, GFP_KERNEL);
	if (sig_t == NULL) {
		kfree(out_t);
		return NULL;
	}
	out = kmalloc(RSA_LEN, GFP_KERNEL);
	if (out == NULL) {
		kfree(out_t);
		kfree(sig_t);
		return NULL;
	}
	key_tmp = kmalloc(RSA_LEN, GFP_KERNEL);
	if (key_tmp == NULL) {
		kfree(out_t);
		kfree(sig_t);
		kfree(out);
		return NULL;
	}

	for (i = 0; i < RSA_LEN; i++)
		sig_t[i] = sig[RSA_LEN - 1 - i];
	for (i = 0; i < RSA_LEN; i++)
		key_tmp[i] = n[RSA_LEN - 1 - i];
	RSADecryption65537((uint32_t *) sig_t,
		(uint32_t *) key_tmp, (uint32_t *) out_t);
	for (i = 0; i < RSA_LEN; i++)
		out[i] = out_t[RSA_LEN - 1 - i];

	kfree(out_t);
	kfree(sig_t);
	kfree(key_tmp);

	return out;
}
