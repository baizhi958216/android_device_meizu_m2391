// SPDX-License-Identifier: Apache-2.0
package stocketc

import (
    "android/soong/android"
    "android/soong/etc"
)

// PrebuiltEtc normally has no source counterpart. The stock policy and hardware
// configuration do have source counterparts, so use Soong's prebuilt selection
// to suppress their install rules instead of overriding output files in Make.
type stockEtc struct {
    etc.PrebuiltEtc
    prebuilt android.Prebuilt
    common bool
}

func init() {
    android.RegisterModuleType("m2391_prebuilt_etc", archFactory)
    android.RegisterModuleType("m2391_prebuilt_etc_common", commonFactory)
}

func (p *stockEtc) Prebuilt() *android.Prebuilt { return &p.prebuilt }
func (p *stockEtc) Name() string { return p.prebuilt.Name(p.PrebuiltEtc.Name()) }
func (p *stockEtc) ImageMutatorSupported() bool { return !p.common }

func factory(common bool) android.Module {
    module := &stockEtc{common: common}
    etc.InitPrebuiltEtcModule(&module.PrebuiltEtc, "etc")
    multilib := android.MultilibFirst
    if common { multilib = android.MultilibCommon }
    android.InitAndroidArchModule(module, android.DeviceSupported, multilib)
    android.InitDefaultableModule(module)
    // PrebuiltEtc validates and copies src itself; no second src property.
    android.InitPrebuiltModuleWithoutSrcs(module)
    return module
}

func archFactory() android.Module { return factory(false) }
func commonFactory() android.Module { return factory(true) }
