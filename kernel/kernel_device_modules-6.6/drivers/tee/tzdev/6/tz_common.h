/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __TZ_COMMON_H__

#ifndef DOXYGEN
#define __TZ_COMMON_H__
#endif

#ifndef __USED_BY_TZSL__
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/limits.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
#include <crypto/sha2.h>
#else
#include <crypto/sha.h>
#endif
#else
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

/**
 * @defgroup IOCTL Android ioctl API
 *
 * The ioctl() system call manipulates the underlying device parameters of special files.
 * In particular, many operating characteristics of character special files (e.g., terminals)
 * may be controlled with ioctl() requests.
 * The argument fd must be an open file descriptor.
 *
 * @{
 */

#ifdef DOXYGEN
/**
 * @brief Manipulates the underlying device parameters of special files.
 * @param[in]     fd      Open file descriptor.
 * @param[in]     request Device-dependent request code.
 * @param[in,out] request_arg   optional parameters associated with request.
 * @return usually, on success zero is returned, on error -1 is returned and
 * errno is set appropriately. Strictly speaking, return status is device-dependent.
 */
int ioctl(int fd, unsigned long request, char *request_arg);
#endif

/**@}*/

/**
 * @def TZ_IOC_MAGIC
 *
 * @brief Magic number for ioctls
 */

#define TZ_IOC_MAGIC			'c'

/**
 * @defgroup UIWSHMEM User-space IW shared memory
 *
 * tziwshmem device allows you to create a shared memory between Normal and Secure world.
 * To do this, the device provides the following file operations: ioctl, mmap.
 *
 * @section tziwshmem_ioctl ioctl
 * ioctl has a single TZIO_MEM_REGISTER command that creates a shared memory of a given size
 * with a given write access to this memory. This call returns id for the created memory
 * You can create shared memory only once for each file descriptor\n 
 *
 * @section tziwshmem_mmap mmap
 * mmap allows you to map the memory that was created by ioctl into the virtual space of the process.
 * mmap has the behavior as described in man, however, it has one feature - the offset parameter
 * must always be zero, otherwise the function will return an error -EINVAL.
 *
 * @{
 */

/**
 * @def TZIO_MEM_REGISTER
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ creates new shred memory region\n  
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Request code to create interworld shared memory region
 * @param[in] request_arg information about interworld shared memory (struct tzio_mem_register)
 * @retval           >=0 in case of success, identifier of created memory region
 * @retval          -1 in case of error, errno contains actual error code
 */
#define TZIO_MEM_REGISTER		_IOW(TZ_IOC_MAGIC, 120, struct tzio_mem_register)

/**@}*/

/**
 * @defgroup TZDEV Common tzdev ioctl requests
 * @{
 *
 * tzdev device allows you to get system configuration, turn off/on CPU boost and control crypto clock
 *
 * @section tzdev_ioctl ioctl
 * ioclt has following _request_ options:
 *
 * - TZIO_GET_SYSCONF
 * - TZIO_BOOST_ON
 * - TZIO_BOOST_OFF
 *
 * Documentation for these requests can be found in following section
 */

/**
 * @def TZIO_GET_SYSCONF
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ requests system config.\n 
 * Other parameters of @ref ioctl and return value are described below\n 
 *
 * @brief Get system confid
 * @param[in] request_arg    pointer to struct tzio_sysconf object
 * @retval           0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_GET_SYSCONF		_IOW(TZ_IOC_MAGIC, 124, struct tzio_sysconf)

/**
 * @def TZIO_BOOST_ON
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ turns on CPU boost\n 
 * Other parameters of @ref ioctl and return value are described below\n 
 *
 * @brief Request CPU boost
 * @retval           0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_BOOST_ON			_IOW(TZ_IOC_MAGIC, 125, int)

/**
 * @def TZIO_BOOST_OFF
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ turns off CPU boost\n 
 * Other parameters of @ref ioctl and return value are described below\n 
 *
 * @brief Turn off CPU boost
 * @retval           0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_BOOST_OFF			_IOW(TZ_IOC_MAGIC, 126, int)

/**@}*/

/* Do not document them, since it's debug */
#define TZIO_PMF_INIT                  _IOW(TZ_IOC_MAGIC, 127, struct tzio_pmf_init)
#define TZIO_PMF_FINI                  _IOW(TZ_IOC_MAGIC, 128, int)
#define TZIO_PMF_GET_TICKS             _IOW(TZ_IOC_MAGIC, 129, int)

/**
 * @defgroup UIWSOCK User-space IW sockets
 *
 * tziwsock device allows you to manipulate iter-world sockets. This device provides APIs similar
 * to POSIX. To do this, the device provides the following file operations: ioctl and poll.
 *
 * @section tziwsock_ioctl ioctl
 * ioctl has bunch of requests. These requests allow to create socket, listen on socket, apply
 * new connection and change socket options. Each of the requests accept file descriptor associated
 * with iterworld socket and does requested operation
 *
 * @section tziwshmem_poll poll
 * poll allows to wait for an event on iter-world socket. For an example caller can wait for new data
 * to come in. poll has the behavior as described in man. It accepts array of poll_table objects and
 * returns positive number on success. This number is an events which actually happened on specified
 * socket. Otherwise -1 and returned and errno is set to an actual error code
 *
 * @{
 */

/**
 * @def TZIO_UIWSOCK_CONNECT
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ connects iter-world socket by the informaiton passed in _request_arg_ to\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Connect user socket
 * @param[in] request_arg  connection information (struct tz_uiwsock_connection)
 * @retval          0 in case of success
 * @retval         -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_CONNECT		_IOW(TZ_IOC_MAGIC, 130, int)

/**
 * @def TZIO_UIWSOCK_WAIT_CONNECTION
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ waits for the connection from secure-world.\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @ref TZIO_UIWSOCK_CONNECT
 * @brief Wait connection completion. It is used in case when connect was interrupted.
 * @retval          0 in case of success
 * @retval         -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_WAIT_CONNECTION	_IOW(TZ_IOC_MAGIC, 131, int)

/**
 * @def TZIO_UIWSOCK_SEND
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ sends data passed in _request_arg_ to secure world\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Send data through socket
 * @param[in] request_arg  pointer to user buffer with data to be sent over socket (struct struct tz_uiwsock_data)
 * @retval            >=0 Number of bytes send
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_SEND		_IOW(TZ_IOC_MAGIC, 132, int)

/**
 * @def TZIO_UIWSOCK_RECV_MSG
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ receives data from secure world into buffer passed in _request_arg_\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Receive message from socket
 * @param[out] request_arg pointer to user buffer for received message (struct tz_msghdr)
 * @retval            >=0 Number of bytes received
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_RECV_MSG		_IOWR(TZ_IOC_MAGIC, 133, int)

/**
 * @def TZIO_UIWSOCK_LISTEN
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ marks socket as listening.\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Mark socket as listening one and bind name specified by user
 * @param[in] request_arg   pointer to user buffer with name to be bind (struct tz_uiwsock_connection)
 * @retval            0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_LISTEN		_IOWR(TZ_IOC_MAGIC, 134, int)

/**
 * @def TZIO_UIWSOCK_ACCEPT
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ marks accepts new connection from secure world.\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Accept connection request
 * @retval            fd of new connected socket in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_ACCEPT		_IOWR(TZ_IOC_MAGIC, 135, int)

/**
 * @def TZIO_UIWSOCK_GETSOCKOPT
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ returns socket options into buffer passed in _request_arg_\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Get socket options
 * @param[out] request_arg   pointer to user buffer for socket information (struct tz_uiwsock_sockopt)
 * @retval            0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_GETSOCKOPT		_IOWR(TZ_IOC_MAGIC, 136, int)

/**
 * @def TZIO_UIWSOCK_SETSOCKOPT
 *
 * This define is used as request parameter of @ref ioctl system call\n 
 * @ref ioctl with this parament in _request_ sets socket options from options passed in _request_arg_\n 
 * Other parameters of @ref ioctl and return value are described below
 *
 * @brief Set socket options
 * @param[in]  request_arg   pointer to user buffer with updated socket information (struct tz_uiwsock_sockopt)
 * @retval            0 in case of success
 * @retval           -1 in case of error, errno contains actual error code
 */
#define TZIO_UIWSOCK_SETSOCKOPT		_IOWR(TZ_IOC_MAGIC, 137, int)

/**@}*/

/* NB: Sysconf related definitions should match with those in SWd */

/**
 * @def SYSCONF_SWD_VERSION_LEN
 * @brief Size of SWD OS  version
 */
#define SYSCONF_SWD_VERSION_LEN			256

/**
 * @def SYSCONF_SWD_EARLY_SWD_INIT
 * @brief If set then SWD uses memory region inside boot area for TZAR
 */
#define SYSCONF_SWD_EARLY_SWD_INIT		(1 << 0)

/**
 * @def SYSCONF_NWD_CPU_HOTPLUG
 * @brief Indicates if NWD hotplug callback is registered
 */
#define SYSCONF_NWD_CPU_HOTPLUG			(1 << 1)

/**
 * @def SYSCONF_NWD_TZDEV_DEPLOY_TZAR
 * @brief Indicates if TZAR image was deployed
 */
#define SYSCONF_NWD_TZDEV_DEPLOY_TZAR		(1 << 2)

#ifndef SHA256_DIGEST_SIZE
/**
 * @def SHA256_DIGEST_SIZE
 * @brief Size of SHA356 digest
 */
#define SHA256_DIGEST_SIZE			32
#endif

#ifndef CRED_HASH_SIZE
/**
 * @def CRED_HASH_SIZE
 * @brief Size of hash for credentials
 */
#define CRED_HASH_SIZE				SHA256_DIGEST_SIZE
#endif

/**
 * @brief Cluster types
 */
enum cluster_type {
	CPU_CLUSTER_BIG,	/**< Cluster type big */
	CPU_CLUSTER_MIDDLE,	/**< Cluster type middle */
	CPU_CLUSTER_LITTLE,	/**< Cluster type little */
	CPU_CLUSTER_MAX,	/**< Wrong type for sanity checking */
};

/**
 * @brief This structure describes cluster
 */
struct sysconf_cluster_info {
	uint32_t type;		/**< Cluster type (see enum cluster_type) */
	uint32_t nr_cpus;	/**< Number of cpus in cluster */
	uint64_t mask;		/**< Available CPUs in cluster */
} __attribute__((__packed__));

/**
 * @brief This structure describes system configuration for Secure World
 */
struct tzio_swd_sysconf {
	uint32_t os_version;						/**< SWd OS version */
	uint32_t cpu_num;						/**< SWd number of cpu supported */
	uint32_t flags;							/**< SWd flags */
	uint32_t nr_clusters;						/**< Number of CPU clusters */
	struct sysconf_cluster_info cluster_info[CPU_CLUSTER_MAX];	/**< Cluster layout */
	char version[SYSCONF_SWD_VERSION_LEN];				/**< SWd OS version string */
} __attribute__((__packed__));

/**
 * @brief This structure describes system configuration for Normal World
 */
struct tzio_nwd_sysconf {
	uint32_t iwi_event;			/**< NWd interrupt to notify about SWd event */
	uint32_t iwi_panic;			/**< NWd interrupt to notify about SWd panic */
	uint32_t flags;				/**< NWd flags */
} __attribute__((__packed__));

/**
 * @brief This structure describes system configuration
 */
struct tzio_sysconf {
	struct tzio_nwd_sysconf nwd_sysconf;	/**< Normal world system config */
	struct tzio_swd_sysconf swd_sysconf;	/**< Secure world system config */
} __attribute__((__packed__));


#ifndef DOXYGEN
struct tzio_pmf_init {
	const uint64_t ptr;
	uint32_t id;
} __attribute__((__packed__));
#endif

/**
 * @brief This structure describes a region of shared memory between worlds
 */
struct tzio_mem_register {
	uint64_t size;	/**< Memory region size (in bytes) */
	uint32_t write;	/**< Write access: 1 - rw, 0 - ro */
} __attribute__((__packed__));

#ifndef __USED_BY_TZSL__
/**
 * @brief This structure describes Trusted Applicatio UUID
 */
struct tz_uuid {
	uint32_t time_low;		/**< integer giving the low 32 bits of the time  */
	uint16_t time_mid;		/**< integer giving the middle 16 bits of the time */
	uint16_t time_hi_and_version;	/**< 4-bit "version" in the most significant bits, followed by the high 12 bits of the time */
	uint8_t clock_seq_and_node[8];	/**< 1 to 3-bit "variant" in the most significant bits, followed by the 13 to 15-bit clock sequence  */
} __attribute__((__packed__));

/**
 * @brief Credential type
 */
enum {
	TZ_CRED_UUID,	/**< Credential UUId */
	TZ_CRED_HASH,	/**< Credential hash */
	TZ_CRED_KERNEL	/**< Credential kernel */
};

/**
 * @def TZ_CRED_HASH_SIZE
 * @brief Hash size for tz_cred structure
 */
#define TZ_CRED_HASH_SIZE 32

/**
 * @brief This structure describes credentials
 */
struct tz_cred {
	uint32_t pid;	/**< Proccess ID */
	uint32_t uid;	/**< User ID */
	uint32_t gid;	/**< Group ID */

	/**
	* @brief Raw Credential. Actual data depends on type member
	*/
	union {
		struct tz_uuid uuid;			/**< UUID */
		uint8_t hash[TZ_CRED_HASH_SIZE];	/**< Hash */
	};
	uint32_t type; /**< Credentials type  */
} __attribute__((__packed__));

#else /* __USED_BY_TZSL__ */
#  include <tz_cred.h>
#  include <tz_uuid.h>
#endif /* __USED_BY_TZSL__ */

/**
 * @brief Structure describing received message
 */
struct tz_msghdr {
	char		*msgbuf; /**< user space address of buffer for message data */
	unsigned long	msglen;  /**< size of buffer for message data */
	char		*msg_control; /**< user space address of buffer for ancillary information (like creds) */
	unsigned long	msg_controllen; /**< size of buffer for ancillary information */
	int		msg_flags; /**< flags controlling transfer */
};

#ifdef CONFIG_COMPAT
struct compat_tz_msghdr {
	uint32_t	msgbuf;
	uint32_t	msglen;
	uint32_t	msg_control;
	uint32_t	msg_controllen;
	int32_t		msg_flags;
};
#endif

/**
 * @brief Structure describing socket to connect to during connect operation
 */
struct tz_cmsghdr {
	uint32_t cmsg_len;	/**< data byte count, including header */
	uint32_t cmsg_level;	/**<  originating protocol */
	uint32_t cmsg_type;	/**<  protocol-specific type */
	/* followed by the actual control message data */
};

/**
 * @def TZ_CMSG_ALIGN
 * @brief Aligns size to uint32_t
 *
 * @param[in]  LEN   size to align
 * @retval            aligned size
 */
#define TZ_CMSG_ALIGN(LEN)		(((LEN) + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1))

/**
 * @def TZ_CMSG_DATA
 * @brief Returns pointer to data inside a message
 *
 * @param[in]  CMSG   Pointer to message
 * @retval            aligned size
 */
#define TZ_CMSG_DATA(CMSG)		((void *)((char *)(CMSG) + TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr))))

/**
 * @def TZ_CMSG_SPACE
 * @brief Returns full size of CMSG message
 *
 * @param[in]  LEN    Size of payload
 * @retval            full size
 */
#define TZ_CMSG_SPACE(LEN)		(TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr)) + TZ_CMSG_ALIGN(LEN))

static inline
struct tz_cmsghdr *TZ_CMSG_FIRSTHDR(const struct tz_msghdr *msg)
{
	return ((msg->msg_controllen >= sizeof(struct tz_cmsghdr)) ? (struct tz_cmsghdr *)msg->msg_control : NULL);
}


/**
 * @def TZ_CMSG_LEN
 * @brief Returns size of full message
 *
 * @param[in]  LEN    Size of payload
 * @retval            full size
 */
#define TZ_CMSG_LEN(LEN)		(TZ_CMSG_ALIGN(sizeof(struct tz_cmsghdr)) + (LEN))

/**
 * @brief Returns pointer to next CMSG header
 *
 * @param[in] msg     pointer to MSG header
 * @param[in] cmsg    pointer to CMSG header
 * @retval            pointer to next CMSG header
 */
static inline
struct tz_cmsghdr *TZ_CMSG_NXTHDR(const struct tz_msghdr *msg, const struct tz_cmsghdr *cmsg)
{
	struct tz_cmsghdr *ret;
	const uint32_t cmsg_len = cmsg->cmsg_len;
	const void *ctl = msg->msg_control;
	const size_t size = msg->msg_controllen;

	if (cmsg_len < sizeof(struct tz_cmsghdr))
		return NULL;

	ret = (struct tz_cmsghdr *)(((unsigned long)cmsg) + TZ_CMSG_ALIGN(cmsg_len));
	if ((size_t)((unsigned long)(ret + 1) - (unsigned long)ctl) > size)
		return NULL;

	return ret;
}

/**
 * @brief Structure containing socket and credentials
 */
struct tz_cmsghdr_cred {
	struct tz_cmsghdr cmsghdr;	/**< Socket to connect */
	struct tz_cred cred;		/**< Credentials */
};

/**
 * @def TZ_UIWSOCK_MAX_NAME_LENGTH
 *
 * @brief Maximum name length for iw socket connection
 */
#define TZ_UIWSOCK_MAX_NAME_LENGTH	256

/**
 * Structure describing socket to connect to during connect operation
 */
struct tz_uiwsock_connection {
	char name[TZ_UIWSOCK_MAX_NAME_LENGTH]; /**< name of listening in SWd socket to connect to. */
};

/**
 * Structure describing user buffer for data transfer through socket
 */
struct tz_uiwsock_data {
	uint64_t buffer; /**< user space address of buffer. */
	uint64_t size;   /**< size of the buffer */
	uint32_t flags;  /**< flags of requested operation */
} __attribute__((__packed__));

/**
 * Structure of socket options to get/set
 */
struct tz_uiwsock_sockopt {
	int32_t level;    /**< protocol level which option belongs to */
	int32_t optname;  /**< option name */
	uint64_t optval;  /**< address buffer for option value */
	uint64_t optlen;  /**< size of option buffer or size of returned data */
} __attribute__((__packed__));

#endif /*__TZ_COMMON_H__*/
