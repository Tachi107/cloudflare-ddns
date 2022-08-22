<!--
SPDX-FileCopyrightText: 2022 Andrea Pappacoda
SPDX-License-Identifier: CC-BY-SA-4.0
-->

# mdoc thoughts

Writing manual pages is not that painful after all, and I think that they do a
lot of stuff the right way. I'm talking about mdoc specifically here, as I have
no experience in using the groff language, or whatever it is.

As opposed to Markdown, mdoc almost doesn't touch the markup part of things,
i.e. you don't really write how you want your manual page to look like, you
write the meaning of the manual: you don't say "I want command line options to
be rendered in bold", you say "this string of text is a command line option",
and mdoc takes care of rendering it however it thinks it is appropriate.

This is really similar to the Word vs Latex thing, or more broadly WYSIWYG vs
WYSIWYM (What You See Is What You Get/Mean). In Word you spend all your time
worrying about making small graphical tweaks to your documents, while in Latex
you spend your time giving semantic meaning to every little string of characters
you insert in the document. Of course these approaches each have their own pros
and cons: Word is really user friendly and you can easily position stuff where
you prefer, but if you decide that it would be better to render all the key
words in italic instead of bold you have to do so manually throrough the whole
document; Latex on the other hand is way more flexible, and changing the style
document-wide is trivial, but it takes quite some time to learn the syntax
required to semantically mark text, and if you want to position something in a
not-so-standard position you may end up insane (I'm looking at you, Float).

So, getting back to manpages, we've had these semantic languages like mdoc for
ages, and they definetly work, but the tidiousness bound to learn the syntax
and bear with the historical debt of the troff language have pissed so many
people off that several meta-languages to write manpages in a more easy manner
emerged, like scdoc and ronn.

These new tools reduced manpages complexity by getting rid almost completely of
all the sematic aspect of their predecessors, and instead gave the writer the
ability to directly mark things up as they prefer, with a limited, concise, and
easy to learn set of special characters; ronn uses a subset of markdown to
alter the aspect of text, while scdoc uses an ad-hoc format, albeit very
similar. I love when formats are minimal and even restrict in some ways what
the author can do.

Great, right? Well, yeah, now you can quickly write documents that'll end up
_looking like_ manpages, but that without all the semantic tagging that always
gave manpages their omogeneous, quick to understand, iconic look. Suddently
you'll have manpages where arguments may now look the same as options, where
optional arguments may be marked inconsistently across different pages because
the author forgot what's the common convention or because of personal taste.
No, manpages are not the place where _you_ should decide how things look,
because manpages should look like manpages, and like nothing else. This is not
HTML.

So, should we all reject modernity and avoid using easy to use formats like
scdoc and ronn? Should we all only use plain mdoc/groff? Well, for the time
being, yes, because as far as I know there are no high-level manpage transpilers
that retain mdoc's sematical features.

Not yet.

We need a more modern alternative, one that doesn't have to deal with man's
macro-based approach, where to mark one command line option as such you have to
insert a new line and open a new macro scope. One that acknowledges what
manpages are used for, and is not bloated with macros to toggle font modes and
other crazy stuff like that. One that doesn't have macros specifically designed
to enclose a piece of text in a couple of fking angled brackets, because thanks
but I have a keyboard for a reason. One that doesn't require you to look at the
documentation to understand if a specific piece of text is going to be part of
the macro or not. In short, a sane, concise format made to write man pages in a
semantic way, and nothing else.

This format should be easy to understand for everybody writing command line
utilities or file formats, e.g. it should be enough to document pkgconf(1) and
pc(5), but not necessairly easy to grasp for your mother. Because probably she
isn't going to be writing a manpage to document her grocery list anyway.

I don't really know how to call this format, and I don't even know if I'll ever
implement or design it, but I was thinking about something like miniman or
minidoc; it starts with an "m" because all of this started with me discovering
mdoc, contains "mini" because this should be a small, simple, concise format,
and contains either "man" or "doc" because that's what it's all about. I'm
leaning towards miniman, as while minidoc really reminds me of mdoc (which is a
good thing) miniman is more explicit and really says to newcomers what they are
getting into.

It should borrow a lot from mdoc, like the `.Ar` macro to mark an argument as
such, but instead of _macros_ I'd prefer to have _functions_, so an `Ar()` or
`.Ar()` function.

Functions should to a lot of stuff automatically, to make common things even
more trivial. For example, mdoc's `.Fl` macro is not really smart, and while it
marks your flags as such only handles POSIX-style short options, so you're
forced to prefix your GNU-style long flags with a single hyphen if you want to
make them look like a proper `--long-option`; but hey, look, I just wrote
_look like_ and that's explicit markup is exactly what we want to avoid, so the
`Fl()` function will automatically prefix long options with two hyphens, so that
`Fl(v)` produces `-v`, but `Fl(version)` produces `--version`. Of course this is
not always desired, because some tools like pretty much every C compiler have
long options that start with a single hiphen, so there will be a `Flm()`
(flag manual) function that allows you to be explicit about the number of
hyphens a specific flag will have (so `Fl(v)` becomes `Flm(-v)` and
`Fl(version)` becomes `Flm(--version)`, but please don't do this, I really like
stacking short options like `grep -Eiv`).

Here are a few parts of cloudflare-ddns' manpage in mdoc format

    .\" SPDX-FileCopyrightText: 2022 Andrea Pappacoda
    .Dd 2022-08-22
    .Dt CLOUDFLARE-DDNS 1
    .Os
    .
    .Sh NAME
    .Nm cloudflare-ddns
    .Nd dynamically update a DNS record
    .
    .Sh SYNOPSIS
    .Nm
    .Op Ar api_token record_name
    .Op Fl -config Ar file
    .
    .Sh DESCRIPTION
    .Nm
    is a tool that can be used to dynamically update a DNS record using
    Cloudflare's API.
    .Pp
    This tool is a oneshot program: you run it, it updates the DNS record, and
    it terminates. To make it run periodically you could use a systemd timer or
    a cron job.
    .
    .Sh EXAMPLES
    .Bd -literal
    $ cloudflare-ddns
    New IP: 149.20.4.15
    .Ed
    .
    .Sh SEE ALSO
    .Xr cron 8
    .Xr systemd.timer 5

And here's how it could look in miniman:

    ; SPDX-FileCopyrightText: 2022 Andrea Pappacoda
    CLOUDFLARE-DDNS(1)
    2022-08-22

    # NAME
    Nm(cloudflare-ddns) Nd(dynamically update a DNS record)

    # SYNOPSIS
    Nm() Op(Ar(api_token record_name)) Op(Fl(config Ar(file)))

    # DESCRIPTION
    Nm() is a tool that can be used to dynamically update a DNS record using
    Cloudflare's API.

    This tool is a oneshot program: you run it, it updates the DNS record, and
    it terminates. To make it run periodically you could use a systemd timer or
    a cron job.

    # EXAMPLES

        $ cloudflare-ddns
        New IP: 149.20.4.15

    # SEE ALSO
    cron(8), systemd.timer(5)

The two are similar, but miniman feels less clunky. You can break a paragraph
by simply doing it, you are not required to insert a newline if you want to
call a new macro, ehm, function, sections are clearly distinct from the other
code, it is easy to tell when and where one funcion ends, code blocks follow
the same minimal format seen in the original Markdown syntax and in emails,
man pages are cross referenced automatically. miniman is familiar to mdoc users,
and almost easy to follow for newcomers. And all of this without giving up
semantics and the iconic man look.
