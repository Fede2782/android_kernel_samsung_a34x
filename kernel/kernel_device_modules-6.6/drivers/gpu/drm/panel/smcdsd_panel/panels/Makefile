smcdsd_panel-y := ../smcdsd_panel_init.o ../smcdsd_panel_base.o ../smcdsd_board.o ../smcdsd_notify.o ../smcdsd_abd.o ../smcdsd_dsi_msg.o

smcdsd_panel-$(CONFIG_SMCDSD_DPUI)	+= dpui.o
smcdsd_panel-$(CONFIG_SMCDSD_MDNIE)	+= mdnie.o

smcdsd_panel-$(and $(CONFIG_SMCDSD_LCD_DEBUG),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_param.o

smcdsd_panel-$(CONFIG_SMCDSD_PANEL_M53X)	+= m53x_sw83109_boe.o
smcdsd_panel-$(CONFIG_SMCDSD_PANEL_A34X)	+= a34x_s6e3fc5_sdc.o

obj-$(CONFIG_SMCDSD_PANEL)	+= smcdsd_panel.o

