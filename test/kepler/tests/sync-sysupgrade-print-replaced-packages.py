self.description = "Print replacements when using -Sup"


lp = pmpkg("a", "1-1")
self.addpkg2db("local", lp)

sp = pmpkg("b", "2-2")
sp.replaces = ["a"]
self.addpkg2db("sync", sp)

self.args = "-Sup"

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=%s-%s" % (sp.name, sp.version))
