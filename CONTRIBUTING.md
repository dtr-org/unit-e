Contributing to UnitE Core
============================

The UnitE Core project operates an open contributor model where anyone is
welcome to contribute towards development in the form of peer review, testing
and patches. This document explains the practical process and guidelines for
contributing.

There is a team of [maintainers](MAINTAINERS.md) who take care of
responsibilities such as merging pull requests, releasing, moderation, and
appointment of maintainers. Maintainers are part of the overall community and
there is a path for contributors to become maintainers if they show to be
capable and willing to take over this responsibility.

The UnitE team is committed to fostering a welcoming and harassment-free
environment. All participants are expected to adhere to our [code of
conduct](CODE_OF_CONDUCT.md).


Contributor Workflow
--------------------

The codebase is maintained using the "contributor workflow" where everyone
without exception contributes patch proposals using GitHub pull requests. This
facilitates social contribution, easy testing and peer review.

We treat the term code generously in this context and apply it not only to the
program code itself but to everything which is stored in the code repository,
including documentation.

### Workflow overview

To contribute a patch, the workflow is as follows:

  1. Fork repository
  1. Create topic branch
  1. Commit patches
  1. Push changes to your fork
  1. Create pull request
  1. Code review
  1. Merging or closing the pull request

The following sections explain the details of the different steps.

### Fork repository

To start a new patch fork the unit-e repository on GitHub or, if you already
have a fork, [update its master
branch](https://help.github.com/articles/syncing-a-fork/) to the latest version.

This workflow is the same for everybody including those who have write access to
the main repo to have a consistent, symmetric, and fair workflow. So don't
create pull requests as branches in the main repo.

### Create topic branch

Create a topic branch in your fork to add your changes there. This makes it
easier to work on multiple changes in parallel and to track the main repo in the
master branch.

### Commit patches

The project coding conventions in the [developer notes](doc/developer-notes.md)
must be adhered to.

In general [commits should be
atomic](https://en.wikipedia.org/wiki/Atomic_commit#Atomic_commit_convention)
and diffs should be easy to read. For this reason do not mix any formatting
fixes or code moves with actual code changes.

Commit messages should be verbose by default consisting of a short subject line
(50 chars max), a blank line and detailed explanatory text as separate
paragraph(s), unless the title alone is self-explanatory (like "Corrected typo
in init.cpp") in which case a single title line is sufficient. Commit messages
should be helpful to people reading your code in the future, so explain the
reasoning for your decisions. See further explanation in Chris Beams' excellent
post ["How to write a commit message"](http://chris.beams.io/posts/git-commit/).

If a particular commit references another issue, please add the reference. For
example: `refs #1234` or `fixes #4321`. Using the `fixes` or `closes` keywords
will cause the corresponding issue to be closed when the pull request is merged.

Please refer to the [Git manual](https://git-scm.com/doc) for more information
about Git.

#### Sign your work

UnitE has adopted the [Developer Certificate of Origin
(DCO)](https://developercertificate.org/) (see
[ADR-16](https://github.com/dtr-org/unit-e-docs/blob/master/adrs/2018-10-22-ADR-16-Adopt%20DCO.md)
for details). That means if you submit a change you sign it off by adding a line

    Signed-off-by: Random J Developer <random@developer.example.org>

with your name and email address at the end of every commit message. By adding
the "Signed-off-by" you state that you have have the right to contribute this change
and that you do so under the MIT license. The full statement you agree to is the
[Developer Certificate of Origin](https://developercertificate.org/):

    Developer Certificate of Origin
    Version 1.1

    Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
    1 Letterman Drive
    Suite D4700
    San Francisco, CA, 94129

    Everyone is permitted to copy and distribute verbatim copies of this
    license document, but changing it is not allowed.


    Developer's Certificate of Origin 1.1

    By making a contribution to this project, I certify that:

    (a) The contribution was created in whole or in part by me and I
        have the right to submit it under the open source license
        indicated in the file; or

    (b) The contribution is based upon previous work that, to the best
        of my knowledge, is covered under an appropriate open source
        license and I have the right under that license to submit that
        work with modifications, whether created in whole or in part
        by me, under the same open source license (unless I am
        permitted to submit under a different license), as indicated
        in the file; or

    (c) The contribution was provided directly to me by some other
        person who certified (a), (b) or (c) and I have not modified
        it.

    (d) I understand and agree that this project and the contribution
        are public and that a record of the contribution (including all
        personal information I submit with it, including my sign-off) is
        maintained indefinitely and may be redistributed consistent with
        this project or the open source license(s) involved.

When you are passing on patches or are merging changes contributed by others
preserve the "Signed-off-by" lines at the end of the commit messages and add
your own one.

You can use git to add the message for you by using the `-s` or `--signoff`
option:

    git commit -s -m "My commit message"

This can be automated using a git hook. Copy the script at
`contrib/githooks/prepare-commit-msg` to your local `.git/hooks` directory and
make sure it's executable.

### Create pull request

Open a pull request in the GitHub UI from your branch to the main repository.

Add any additional context or references which are relevant to the process of
reviewing the changes to the body of the pull request. Information relevant to
understanding the changes itself should be in the code and commit messages.

Assign reviewers if you want to get feedback from specific people. Generally
everybody is free to comment on any pull request. It's expected from assigned
reviewers to give feedback on the pull request. They might remove themselves or
add others if they feel that they can't approve the pull request themselves.

Patchsets should always be focused. For example, a pull request could add a
feature, fix a bug, or refactor code; but not a mixture. Please also avoid super
pull requests which attempt to do too much, are overly large, or overly complex
as this makes review difficult.

If a pull request is not to be considered for merging (yet), please prefix the
title with `WIP:`. You can use [Tasks
Lists](https://help.github.com/articles/basic-writing-and-formatting-syntax/#task-lists)
in the body of the pull request to indicate tasks are pending.

If you continue to work in the pull request and it gets to a state where it's
ready to be merged, remove the `WIP:` and notify reviewers that it's ready to be
reviewed for merge.

Use this mechanism also to get early feedback on concepts or incomplete
implementations which aren't integrated with the overall code yet. Create a pull
request on master with the `WIP:` prefix in the title so the discussion is
available in the main project but it's clear that the code doesn't get merged as
it is.

#### Features

When adding a new feature, thought must be given to the long term maintenance
that feature may require after inclusion. Before proposing a new feature that
will require maintenance, please consider if you are willing to maintain it
(including bug fixing).

#### Refactoring

Refactoring is a necessary part of any software project's evolution. The
following guidelines cover refactoring pull requests for the project.

There are three categories of refactoring, code only moves, code style fixes,
code refactoring. In general refactoring pull requests should not mix these
three kinds of activity in order to make refactoring pull requests easy to
review and uncontroversial. In all cases, refactoring PRs must not change the
behaviour of code within the pull request (bugs must be preserved as is).

Project maintainers aim for a quick turnaround on refactoring pull requests, so
where possible keep them short, uncomplex and easy to verify.

### Code Review

Code review is essential to keep up code quality. It also is a great way to
learn and to positively collaborate. Good code review improves the code and the
team.

Some great general resources about code review are [Designing awesome code
reviews](https://medium.com/unpacking-trunk-club/designing-awesome-code-reviews-5a0d9cd867e3)
(an overview of how to do code reviews that are good for code and people) and
[Awesome code review](https://github.com/joho/awesome-code-review) (a curated
list of resources related to code review).

#### Peer Review

Anyone may participate in peer review which is expressed by comments in the pull
request. Use the features of the GitHub review system. Comment on the code,
propose changes, ask questions, and add a summary of your review.

Typically reviewers will review the code for obvious errors, as well as
test out the patch set and opine on the technical merits of the patch. Project
maintainers take into account the peer review when determining if there is
consensus to merge a pull request. The following
language is used within pull-request comments:

  - ACK means "I have tested the code and I agree it should be merged";
  - NACK means "I disagree this should be merged", and must be accompanied by
    sound technical justification (or in certain cases of
    copyright/patent/licensing issues, legal justification). NACKs without
    accompanying reasoning may be disregarded;
  - utACK means "I have not tested the code, but I have reviewed it and it looks
    OK, I agree it can be merged";
  - Concept ACK means "I agree in the general principle of this pull request";
  - Nit refers to trivial, often non-blocking issues.

Where a patch set affects consensus critical code, the bar will be set much
higher in terms of discussion and peer review requirements, keeping in mind that
mistakes could be very costly to the wider community. This includes refactoring
of consensus critical code.

Patches that change UnitE consensus rules are considerably more involved than
normal because they affect the entire ecosystem. They must be accompanied by a
design document and a discussion will have preceded it, which should be
referenced (for example the pull request which merged the design document).

While each case will
be different, one should be prepared to expend more time and effort than for
other kinds of patches because of increased peer review and consensus building
requirements.

#### Adding changes to the pull request

At this stage one should expect comments and review from other contributors. You
can add more commits to your pull request by committing them locally and pushing
to your fork until you have satisfied all feedback.

Add changes as additional commits so that it gives a clear history and that the
discussion in the pull request on GitHub can be followed along the code.

Find a balance between improving an existing pull request or doing changes as a
new pull request after the first one has been merged. We are striving for
quality but not for perfection. Small pull requests help with that because they
make reviews easier and more effective.

When you are rebasing a branch of a pull request to merge in other changes from
master or to clean up commits you need to force push to share your changes. Be
aware that this breaks the history of the pull request on GitHub, might
invalidate or confuse discussions, and forces people who have local checkouts of
the branch to re-checkout. Be careful when force pushing. You might want to
consider opening a new pull request when that makes things clearer without
adding too much effort or noise.

#### Decision by maintainers

The following applies to code changes to the UnitE Core project (and related
projects such as libsecp256k1), and is not to be confused with overall UnitE
Network Protocol consensus changes.

Whether and when a pull request is merged into UnitE Core rests with the project
maintainers. A pull request needs approval of at least one maintainer to be
merged. A maintainer can not approve a pull request they authored themselves.
They need at least one other maintainer to approve the pull request. The list of
maintainers is documented in [MAINTAINERS.md](MAINTAINERS.md).

If maintainers ask for changes, and the changes have been done by adding
additional commits to the pull request, they need to approve the changes before
the pull request can be merged.

Approval is expressed by using the GitHub review mechanics. If, as a maintainer,
you approve the patch for merge, select the "approve" option in the GitHub UI
when submitting your review. If you request changes, select the corresponding
option to require submission and review of additional changes.

Approval of a pull request is a strong statement. It expresses that the approver
takes responsibility that the change will work, does not break anything else,
has sufficient test coverage, and will be maintained in the future. Approving a
pull request expresses the same level of confidence that the change is good to
be shipped as actually merging it.

Maintainers will take into consideration if a patch is in line with the general
principles of the project; meets the minimum standards for inclusion; and will
judge the general consensus of contributors.

In general, all pull requests must:

  - Have a clear use case, fix a demonstrable bug or serve the greater good of
    the project (for example refactoring for modularisation);
  - Be well peer reviewed;
  - Have unit tests and functional tests where appropriate;
  - Follow code style guidelines ([C++](doc/developer-notes.md), [functional
    tests](test/functional/README.md));
  - Not break the existing test suite, all checks such as style, unit, or
    functional tests must pass;
  - Where bugs are fixed, where possible, there should be unit tests
    demonstrating the bug and also proving the fix. This helps prevent
    regression.

There are special areas of the code which need expert review. As a maintainer it
is your responsibility to involve others if you see that the change is touching
these special areas. A definition of who is expert on which area of the code is
in the [MAINTAINERS.md](MAINTAINERS.md) file.

### Merging or closing the pull request

Once approved, a maintainer with write access will merge the pull request.

Pull requests are squashed on merge to keep the history of the code clean and
not pollute it with work in progress commits or artifacts of discussions in the
pull request. The commit message should reflect the outcome of the discussion
and describe what ends up in the commit being merged. The history of the pull
request with its discussion can be looked up on GitHub.

Use the GitHub button for squashing and merging. Be careful with editing the
commit message during the squash as this is what directly will end up in the
main code branch. Copy relevant information from the pull request description as
needed.

There are some exceptions to the rule to squash commits, for example when
merging upstream code and the commits and authors should be kept as they are.


Release Policy
--------------

The project leader is the release manager for each UnitE Core release.

Copyright
---------

By contributing to this repository, you agree to license your work under the
MIT license unless specified otherwise in `contrib/debian/copyright` or at
the top of the file itself. Any work contributed where you are not the original
author must contain its license header with the original author(s) and source.
