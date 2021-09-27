self.description = "Sync a package keeping the existing note"

sp = pmpkg("pkg")
self.addpkg2db("sync", sp)

lp = pmpkg("pkg")
lp.note = "this is a note"
self.addpkg2db("local", lp)

self.args = "-S pkg"

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg")
self.addrule("PKG_NOTE=pkg|this is a note")
