self.description = "Hook using multiple 'Description's"

self.add_hook("hook",
        """
        [Trigger]
        Type = Package
        Operation = Install
        Target = foo

        [Action]
        Description = lala
        Description = foobar
        When = PreTransaction
        Exec = /bin/date
        """);

sp = pmpkg("foo")
self.addpkg2db("sync", sp)

self.args = "-S foo"

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=warning.*overwriting previous definition of Description")
