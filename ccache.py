Import("env")
import os
import shutil

if shutil.which("ccache"):
    env["ENV"]["CCACHE_SLOPPINESS"] = "time_macros,file_macro"
    env["ENV"]["CCACHE_BASEDIR"]    = env.subst("$PROJECT_DIR")
    env.Replace(CC  = "ccache " + env["CC"])
    env.Replace(CXX = "ccache " + env["CXX"])
