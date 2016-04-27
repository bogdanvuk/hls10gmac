from pymake.builds.fileset import FilesetBuild, FileBuild, FileCopyBuild
from pymake.builds.vivado_hls import VivadoHlsProjectBuild, VivadoHlsSolution,\
    VivadoHlsVhdlSynthBuild
from pymake.build import Build, SrcConf
from collections import OrderedDict
import os
import re
from pymake.utils import Fileset
import itertools

def VivadoHlsSynthBuildFactory(module_name):
    prj = VivadoHlsProjectBuild(prj          = FileBuild(os.path.join('$BUILDDIR', module_name)),
                                config       = {'top': module_name},
                                fileset      = FilesetBuild(match=['$SRCDIR/{}/*'.format(module_name),
                                                                    '$SRCDIR/fcs/*'],
                                                            ignore=['$SRCDIR/*/tb/*']),
                                include      = FilesetBuild(['$SRCDIR/include']),
                                tb_fileset   = FilesetBuild(match=['$SRCDIR/{}/tb/*.cpp'.format(module_name)]),
                                solutions    = [VivadoHlsSolution(clock='-period 40 -name default',
                                                                   config={'rtl': '-reset all -reset_level high'})]
                  )
    return VivadoHlsVhdlSynthBuild(prj)

b = Build(*[VivadoHlsSynthBuildFactory(name) for name in ['receive']])
b = FileCopyBuild((FileBuild(os.path.join('$BUILDDIR', 'ip', 'hdl')), b))

def postproc(name, res, key):
    if (len(key) == 2) and (key[1] == 1):
        return Fileset(list(itertools.chain(*res)), list_timestamp=min([f.list_timestamp for f in res]))

    return res

b.srcs_setup['args'].postproc = postproc
prjs = b.build('hlsmac', builddir='../build', srcdir='../src')
pass
