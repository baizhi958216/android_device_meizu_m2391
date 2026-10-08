# Device tree for MEIZU 20 Pro — DerpFest 16.2 / LineageOS 23.2

device tree for the MEIZU 20 Pro (`m2391`).

### Build

```sh
bash device/meizu/m2391/apply-patch.sh
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
export DERPFEST_BUILD_TYPE=Community
lunch "lineage_m2391-$aosp_target_release-userdebug"
mka derp
```

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
