self.description = "Query ownership of file in root"

sp = pmpkg("dummy")
sp.files = ["etc/config"]
self.addpkg2db("local", sp)

self.filesystem = ["config"]

self.args = "-Qo /config"

self.addrule("KEPLER_RETCODE=1")
