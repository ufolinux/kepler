self.description = "Search for package from a sync db"

sp = pmpkg("dummy")
sp.groups = ["group1", "group2"]
sp.files = ["bin/dummy",
            "usr/man/man1/dummy.1"]
self.addpkg2db("sync", sp)

self.args = "-Ss %s" % sp.name

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=^sync/%s" % sp.name)
