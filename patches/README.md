# Zephyr SDK patches (NCS v3.0.0)

This app needs `CONFIG_BT_ID_AUTO_SWAP_MATCHING_BONDS`, a Bluetooth host
Kconfig option that Nordic added upstream (nrfconnect/sdk-zephyr#2963) after
the NCS v3.0.0 snapshot. It is **not** part of the stock v3.0.0 `zephyr`
checkout, so it has to be backported into the SDK itself before this app can
build with the option enabled.

`0001-nrf-noup-bluetooth-host-Add-support-for-bonding-wit.patch` backports it
onto `D:\ncs\v3.0.0\zephyr` (touches `subsys/bluetooth/host/{Kconfig,adv.c,
adv.h,hci_core.h,id.c,id.h,keys.c,keys.h,smp.c}`). It has already been applied
and committed there, on a local branch named
`nrf-noup-bt-id-auto-swap-v3.0.0` (based on the `manifest-rev` commit that
`west` normally checks out).

**This patch lives only in that local SDK checkout.** `west update` resets
`zephyr/` back to `manifest-rev` and will silently drop it. If that happens,
reapply with:

```sh
cd D:\ncs\v3.0.0\zephyr
git checkout manifest-rev -b nrf-noup-bt-id-auto-swap-v3.0.0   # or reuse the branch if it still exists
git am D:\ncs\example\mutilple_bt_id_swiftpair\patches\0001-nrf-noup-bluetooth-host-Add-support-for-bonding-wit.patch
git checkout nrf-noup-bt-id-auto-swap-v3.0.0   # make sure this is the checked-out ref before building
```
