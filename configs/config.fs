# SPDX-License-Identifier: Apache-2.0
# Vendor accounts and non-default file capabilities/ownership for m2391.

[AID_VENDOR_QTI_DIAG]
value: 2901

[AID_VENDOR_QDSS]
value: 2902

[AID_VENDOR_RFS]
value: 2903

[AID_VENDOR_RFS_SHARED]
value: 2904

[AID_VENDOR_ADPL_ODL]
value: 2905

[AID_VENDOR_QRTR]
value: 2906

[AID_VENDOR_THERMAL]
value: 2907

[AID_VENDOR_FASTRPC]
value: 2908

[AID_VENDOR_QTR]
value: 2909

[AID_VENDOR_NXP_STRONGBOX]
value: 2910

[AID_VENDOR_NXP_WEAVER]
value: 2911

[AID_VENDOR_SSGTZD]
value: 2912

[AID_VENDOR_THALES_STRONGBOX]
value: 2913

[vendor/bin/cnd]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0x1000001400

[vendor/bin/ims_rtp_daemon]
mode: 0755
user: AID_RADIO
group: AID_RADIO
caps: 0x400

[vendor/bin/imsdaemon]
mode: 0755
user: AID_RADIO
group: AID_RADIO
caps: 0x1800000400

[vendor/bin/loc_launcher]
mode: 0755
user: AID_GPS
group: AID_GPS
caps: 0xc0

[vendor/bin/pd-mapper]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0x400

[vendor/bin/pm-service]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0x400

[vendor/bin/sensors.qti]
mode: 0755
user: AID_SYSTEM
group: AID_SYSTEM
caps: 0x400

[vendor/bin/slim_daemon]
mode: 0755
user: AID_GPS
group: AID_GPS
caps: 0x400

[vendor/bin/xtwifi-client]
mode: 0755
user: AID_GPS
group: AID_GPS
caps: 0x1800000400
