Contributing to UnitE Core
============================

The UnitE Core project operates an open contributor model where anyone is
welcome to contribute towards development in the form of peer review, testing
and patches. This document explains the practical process and guidelines for
contributing.

There is a team of maintainers who take care of central responsibilities such as merging pull requests, releasing, moderation, and appointment of maintainers. Maintainers are part of the overall community and there is a path for contributors to become maintainers if they show to be capable and willing to take over this responsibility.


Contributor Workflow
--------------------

The codebase is maintained using the "contributor workflow" where everyone
without exception contributes patch proposals using GitHub pull requests. This
facilitates social contribution, easy testing and peer review.

We treat the term code generously in this context and apply it not only to the program code itself but to everything which is stored in the code repository, including documentation.

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

To start a new patch fork the unit-e repository on GitHub or, if you already have a fork, update its master branch to the latest version.

This workflow is the same for everybody including those who have write access to the main repo to have a consistent, symmetric, and fair workflow. So don't create pull requests as branches in the main repo.

### Create topic branch

Create a topic branch in your fork to add your changes there. This makes it easier to work on multiple changes in parallel and to track the main repo in the master branch.

### Commit patches

The project coding conventions in the [developer notes](doc/developer-notes.md)
must be adhered to.

In general [commits should be atomic](https://en.wikipedia.org/wiki/Atomic_commit#Atomic_commit_convention)
and diffs should be easy to read. For this reason do not mix any formatting
fixes or code moves with actual code changes.

Commit messages should be verbose by default consisting of a short subject line
(50 chars max), a blank line and detailed explanatory text as separate
paragraph(s), unless the title alone is self-explanatory (like "Corrected typo
in init.cpp") in which case a single title line is sufficient. Commit messages should be
helpful to people reading your code in the future, so explain the reasoning for
your decisions. See further explanation in Chris Beams' excellent post ["How to write a commit message"](http://chris.beams.io/posts/git-commit/).

If a particular commit references another issue, please add the reference. For
example: `refs #1234` or `fixes #4321`. Using the `fixes` or `closes` keywords
will cause the corresponding issue to be closed when the pull request is merged.

Please refer to the [Git manual](https://git-scm.com/doc) for more information
about Git.

### Create pull request

Open a pull request in the GitHub UI from your branch to the main repository.

Add any additional context or references which are relevant to the process of reviewing the changes to the body of the pull request. Information relevant to understanding the changes itself should be in the code and commit messages.

Assign reviewers if you want to get feedback from specific people. Generally everybody is free to comment on any pull request.

Patchsets should always be focused. For example, a pull request could add a
feature, fix a bug, or refactor code; but not a mixture. Please also avoid super
pull requests which attempt to do too much, are overly large, or overly complex
as this makes review difficult.

If a pull request is not to be considered for merging (yet), please
prefix the title with [WIP]. You can use [Tasks Lists](https://help.github.com/articles/basic-writing-and-formatting-syntax/#task-lists)
in the body of the pull request to indicate tasks are pending.

If you continue to work in the pull request and it gets to a state where it's ready to be merged, remove the `[WIP]` and notify reviewers that it's ready to be reviewed for merge.

#### Features

When adding a new feature, thought must be given to the long term maintenance that feature may require after inclusion. Before proposing a new
feature that will require maintenance, please consider if you are willing to
maintain it (including bug fixing).

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

Code review is essential to keep up code quality. It also is a great way to learn and to positively collaborate. Good code review improves the code and the team.

Some great general resources about code review are [Designing awesome code reviews](https://medium.com/unpacking-trunk-club/designing-awesome-code-reviews-5a0d9cd867e3) (an overview of how to do code reviews that are good for code and people) and [Awesome code review](https://github.com/joho/awesome-code-review) (a curated list of resources related to code review).

#### Peer Review

Anyone may participate in peer review which is expressed by comments in the pull
request. Use the features of the GitHub review system. Comment on the code, propose changes, ask questions, and add a summary of your review.

Typically reviewers will review the code for obvious errors, as well as
test out the patch set and opine on the technical merits of the patch. Project
maintainers take into account the peer review when determining if there is
consensus to merge a pull request. The following
language is used within pull-request comments:

  - ACK means "I have tested the code and I agree it should be merged";
  - NACK means "I disagree this should be merged", and must be accompanied by
    sound technical justification (or in certain cases of copyright/patent/licensing
    issues, legal justification). NACKs without accompanying reasoning may be
    disregarded;
  - utACK means "I have not tested the code, but I have reviewed it and it looks
    OK, I agree it can be merged";
  - Concept ACK means "I agree in the general principle of this pull request";
  - Nit refers to trivial, often non-blocking issues.

Where a patch set affects consensus critical code, the bar will be set much
higher in terms of discussion and peer review requirements, keeping in mind that
mistakes could be very costly to the wider community. This includes refactoring
of consensus critical code.

Patches that change UnitE consensus rules are considerably more involved than
normal because they affect the entire ecosystem and so must be preceded by
extensive discussions and have a numbered improvement proposal. While each case will
be different, one should be prepared to expend more time and effort than for
other kinds of patches because of increased peer review and consensus building
requirements.

#### Adding changes to the pull request

At this stage one should expect comments and review from other contributors. You
can add more commits to your pull request by committing them locally and pushing
to your fork until you have satisfied all feedback.

Add changes as additional commits so that it gives a clear history and that the discussion in the pull request on GitHub can be followed along the code.

Find a balance between improving an existing pull request or doing changes as a new pull request after the first one has been merged. We are striving for quality but not for perfection. Small pull requests help with that because they make reviews easier and more effective.

Never force push to a review branch as this breaks the history of the pull request on GitHub, might invalidate or confuse discussions, and forces people who have local checkouts of the branch to re-checkout.

In case you want to clean up commits in a pull requests by rebasing, which changes the history, submit them as a new pull request and close the old one. Try to find the right point in time for this when it does not interfere with ongoing discussions or leave the cleanup to the final squash on merge. Reference the old pull request by adding something like `Supersedes #123` in the body of the new pull request.

#### Decision by maintainers

The following applies to code changes to the UnitE Core project (and related
projects such as libsecp256k1), and is not to be confused with overall UnitE
Network Protocol consensus changes.

Whether a pull request is merged into UnitE Core rests with the project maintainers. A pull request needs approval of at least one maintainer to be merged. If changes might have a critical impact, experts on the area of the change have to approve. Maintainers and experts are documented in the MAINTAINERS.md file.

If maintainers ask for changes, and the changes have been done by adding additional commits to the pull request, they need to approve the changes before the pull request can be merged.

Approval is expressed by using the GitHub review mechanics. If, as a maintainer, you approve the patch for merge, select the "approve" option in the GitHub UI when submitting your review. If you request changes, select the corresponding option to require submission and review of additional changes.

Maintainers will take into consideration if a patch is in line with the general
principles of the project; meets the minimum standards for inclusion; and will
judge the general consensus of contributors.

In general, all pull requests must:

  - Have a clear use case, fix a demonstrable bug or serve the greater good of
    the project (for example refactoring for modularisation);
  - Be well peer reviewed;
  - Have unit tests and functional tests where appropriate;
  - Follow code style guidelines ([C++](doc/developer-notes.md), [functional tests](test/functional/README.md));
  - Not break the existing test suite, all checks such as style, unit, or functional tests must pass;
  - Where bugs are fixed, where possible, there should be unit tests
    demonstrating the bug and also proving the fix. This helps prevent regression.

There are special areas of the code which need expert review. As a maintainer it is your responsibility to involve others if you see that the change is touching these special areas. A definition of who is expert on which area of the code is in the MAINTAINERS.md file.

### Merging or closing the pull request

Once approved, somebody with write access can merge the pull request. This should usually not be the author of the pull request, if it came from somebody with write access to the repo.

Pull requests are squashed on merge to keep the history of the code clean and not pollute it with work in progress commits or artifacts of discussions in the pull request. The commit message should reflect the outcome of the discussion and describe what ends up in the commit being merged. The history of the pull request with its discussion can be looked up on GitHub.

Use the GitHub button for squashing and merging.

There are some exceptions to the rule to squash commits, for example when merging upstream code and the commits and authors should be kept as they are.


Release Policy
--------------

The project leader is the release manager for each UnitE Core release.

Copyright
---------

By contributing to this repository, you agree to license your work under the
MIT license unless specified otherwise in `contrib/debian/copyright` or at
the top of the file itself. Any work contributed where you are not the original
author must contain its license header with the original author(s) and source.
