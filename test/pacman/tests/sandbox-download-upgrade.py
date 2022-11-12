self.description = "--upgrade with SandboxUser set"
self.require_capability("curl")

self.option['SandboxUser'] = ['root']

p1 = pmpkg('pkg1', '1.0-1')
self.addpkg(p1)

url = self.add_simple_http_server({
    '/{}'.format(p1.filename()): p1.makepkg_bytes(),
})

self.args = '-U {url}/{}'.format(p1.filename(), url=url)

self.addrule("PACMAN_RETCODE=0")
self.addrule("PKG_EXIST=pkg1")
self.addrule("CACHE_EXISTS=pkg1|1.0-1")
