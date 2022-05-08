self.description = "Query info on a package (new date)"

p = pmpkg("foobar")
p.files = ["bin/foobar"]
p.desc = "test description"
p.groups = ["foo"]
p.url = "http://www.archlinux.org"
p.license = "GPL2"
p.arch = "i686"
# test new style date
p.builddate = "1196640127"
p.packager = "Arch Linux"

self.addpkg2db("local", p)

self.args = "-Qi %s" % p.name

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=^Name.*%s" % p.name)
self.addrule("KEPLER_OUTPUT=^Description.*%s" % p.desc)
self.addrule("KEPLER_OUTPUT=^Build Date.* 2007")
