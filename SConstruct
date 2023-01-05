env = Environment(CCFLAGS = '-g3 -O0 -std=gnu99 -Wall')
env.ParseConfig('pkg-config --cflags --libs gtk+-2.0 poppler-glib')

objBuildDir = 'objs'
srcs = Split(
"""
findbar.c
finddialog.c
index.c
marshal.c
sidebar.c
stock.c
thumbview.c
vsapp.c
vslayout.c
vspage.c
"""
)
srcs = map(lambda x:objBuildDir+'/'+x, srcs)


oenv = env.Clone()
oenv.VariantDir(objBuildDir, 'src', duplicate=0)
objs = oenv.Object(srcs)

env.Program('vspdf', objs)
