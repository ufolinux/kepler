self.description = "Get package list from sync dbs"

sp = pmpkg("dummy")
sp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("sync", sp)

self.args = "-Sl"

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=^sync %s" % sp.name)
