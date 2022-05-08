self.description = "Sysupgrade with a newer sync package"

sp = pmpkg("dummy", "1.0-2")
lp = pmpkg("dummy")

self.addpkg2db("sync", sp)
self.addpkg2db("local", lp)

self.args = "-Su"

self.addrule("KEPLER_RETCODE=0")
self.addrule("PKG_VERSION=dummy|1.0-2")
