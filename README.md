eppipe
======
So, you've got a program like `tail` that doesn't detect when the other
end of whatever is on stdin/stdout (like a pipe, or xinetd) has closed
until it tries to write. How to fix? You could write a
[new version of tail](https://github.com/Roguelazer/einotail). Or you could
just wrap it in eppipe.

Usage
-----
`eppipe tail -f /tmp/foo`
