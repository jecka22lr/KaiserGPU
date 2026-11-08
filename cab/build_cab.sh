#!/bin/sh
# Packs vendor/ + bin/ into KaiserGPU_1.0.cab using _setup.xml. Needs gcab.
set -e
cd "$(dirname "$0")"
S=$(mktemp -d)
cp ../bin/libgles_cm.dll                 $S/KGPU0000.001
cp ../vendor/patched/libgles_cm_ks.dll   $S/KGPU0001.002
cp ../vendor/htc-ca/ahi2dati.dll         $S/KGPU0002.003
cp ../vendor/htc-ca/ahi2dati_dm.dll      $S/KGPU0003.004
cp ../vendor/htc-ca/clkregim.dll         $S/KGPU0004.005
cp ../vendor/htc-ca/msm_clk.dll          $S/KGPU0005.006
cp ../vendor/htc-ca/TrsTai_extensioF.dll $S/KGPU0006.007
cp ../vendor/htc-ca/wce_reF.dll          $S/KGPU0007.008
cp ../vendor/htc-ca/wce_pmeF.dll         $S/KGPU0008.009
cp ../vendor/htc-ca/wce_rex_dm.dll       $S/KGPU0009.010
cp ../vendor/htc-ca/d3dm_ati.dll         $S/KGPU0010.011
cp ../bin/gltest.exe                     $S/KGPU0011.012
cp ../bin/cuberun.exe                    $S/KGPU0012.013
cp _setup.xml $S/
(cd $S && gcab -c out.cab _setup.xml KGPU00*)
mv $S/out.cab KaiserGPU_1.0.cab
rm -rf $S
ls -l KaiserGPU_1.0.cab
