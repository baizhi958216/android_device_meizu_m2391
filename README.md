# device tree for MEIZU 20 Pro

device tree for the MEIZU 20 Pro (`m2391`).

## Build

```sh
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
lunch lineage_m2391 "$aosp_target_release" userdebug
mka bacon
```

## Fingerprint payment

`IFAAService` and `SoterService` are imported from the matching stock dump,
then signed with the ROM's platform certificate and installed in `system_ext`.
Both retain `android.uid.system` and the Meizu/QTI HIDL transports; the existing
`system_app` HAL policy applies. Ordinary fingerprint unlock alone does not
provide these app-facing services.

Keep `ro.product.mobile.name=m2391`: the stock IFAA service derives its payment
model identifier (`MEIZU-M2391`) from it. The APKs remain in their original
`system/app` extraction paths and are pinned in `proprietary-files.txt`;
`Android.bp` selects their installation partition and certificate.

On-device validation on 2026-10-07, with SELinux enforcing: Alipay bound both
services, IFAA v4 detected the enrolled fingerprint, native IFAA commands
returned success, and the user confirmed that the fingerprint-payment option
appeared. A payment transaction was not tested. The Soter HIDL connection works,
but device-ID lookup, ATTK verification and test-app ASK generation returned
`-20` from the native stack; WeChat payment remains unverified. Do not treat
successful service binding as successful key generation.

## Device specifications

| Feature | Specification |
| --- | --- |
| Device | MEIZU 20 Pro |
| Codename | `m2391`; stock OTA identifier: `meizu20Pro` |
| SoC | Qualcomm Snapdragon 8 Gen 2 (`kalama`) |
| GPU | Adreno 740 |
| Memory | 8 GB / 12 GB, depending on variant |
| Storage | 128 GB / 256 GB / 512 GB, depending on variant |
| Display | 6.81-inch OLED, 3200 × 1440, 120 Hz |
| Battery | 5000 mAh (typical), 80 W wired / 50 W wireless charging |
| Rear cameras | 50 MP main + 50 MP ultrawide + 50 MP portrait telephoto |
| Front camera | 32 MP |

<p align="center">
<img src="https://openfile.meizu.com/group1/M00/0B/50/Cgbj0GTkEiiACPvqAAdsxsQKM48695.png" width="500" height="590">
</p>
