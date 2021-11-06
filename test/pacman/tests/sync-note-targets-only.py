self.description = "Make sure note is only set for targets"

sp1 = pmpkg("pkg1", "1.0-2")
sp1.depends = ["pkg2"]

sp2 = pmpkg("pkg2")

sp3 = pmpkg("pkg3")
sp3.depends = ["pkg4"]

sp4 = pmpkg("pkg4")

for p in sp1, sp2, sp3, sp4:
	self.addpkg2db("sync", p)

lp1 = pmpkg("pkg1")
self.addpkg2db("local", lp1)

self.args = "-S pkg1 pkg3 --note aaaa"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("PKG_EXIST=pkg2")
self.addrule("PKG_EXIST=pkg3")
self.addrule("PKG_EXIST=pkg4")

self.addrule("PKG_NOTE=pkg1|aaaa")
self.addrule("PKG_NOTE=pkg2|")
self.addrule("PKG_NOTE=pkg3|aaaa")
self.addrule("PKG_NOTE=pkg4|")
