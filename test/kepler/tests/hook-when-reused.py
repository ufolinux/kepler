self.description = "Hook using multiple 'When's"

self.add_hook("hook",
        """
        [Trigger]
        Type = Package
        Operation = Install
        Target = foo

        [Action]
        When = PreTransaction
        Exec = /bin/date
        When = PostTransaction
        """);

sp = pmpkg("foo")
self.addpkg2db("sync", sp)

self.args = "-S foo"

self.addrule("KEPLER_RETCODE=0")
self.addrule("KEPLER_OUTPUT=warning.*overwriting previous definition of When")
