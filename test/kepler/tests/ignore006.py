self.description = "Sync with target in ignore list and say no"

pkg = pmpkg("package1")
self.addpkg2db("sync", pkg)

self.option["IgnorePkg"] = ["package1"]
self.args = "--ask=1 -S %s" % pkg.name

self.addrule("KEPLER_RETCODE=0")
self.addrule("!PKG_EXIST=package1")
