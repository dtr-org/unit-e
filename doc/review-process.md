# Review process

Code review is essential to keep up code quality. It also is a great way to learn and to positively collaborate. Good code review improves the code and the team.

This document describes how we do code review in the UnitE project. It focuses on the process of the review. Guidelines for the content of commits are described in doc/commit_guidelines.md (*still to be done*).

## General process

Code review is mandatory for all code which goes into UnitE repositories. We treat the term code generously in this context and apply it not only to the program code itself but to everything which is stored in the code repository, including documentation.

All changes are going through the same review process independent of from whom they are coming.

The review is done through GitHub pull requests.

All automatic checks such as style, unit, or functional tests which run on the pull request must pass to consider the pull request for acceptance.

In addition to that a pull request needs approval from at least one maintainer of the project (see MAINTAINERS.md for who those are).

If maintainers ask for changes, and the changes have been done by adding additional commits to the pull request, they need to approve the changes before the pull request can be merged.

Once approved, the pull request is merged to the main branch.

## Submitting a pull request

If you have write access to the repo create a branch in the repo with your changes and create a pull request from this branch.

If you don't have write access for the repo, create a topic branch, and create the pull request to the main repo from the branch of your fork.

Assign reviewers if you want to get feedback from specific people. Generally everybody is free to comment on any pull request.

Add any additional context or references which are relevant to the process of reviewing the changes to the body of the pull request. Information relevant to understanding the changes itself should be in the code and commit messages.

### What makes a great pull request

It's an art to create great pull requests. Here are some things which help:

* Keep it focused on one thing, fixing one bug, adding one feature, refactoring one aspect of the code.
* Make it readable and easy to review, keep it small, provide the context reviewers might need.
* Keep the tests green and add tests for new code.
* Follow the guidelines for style of code and commits.

## Adding changes to a pull request

You might want to add changes to a pull request, as a result of a discussion, because a maintainer asked for it, or because you found an improvement to the current solution.

Add changes as additional commits so that it gives a clear history and that the discussion in the pull request on GitHub can be followed along the code.

Find a balance between improving an existing pull request or doing changes as a new pull request after the first one has been merged. We are striving for quality but not for perfection. Small pull requests help with that because they make reviews easier and more effective.

Never force push to a review branch as this breaks the history of the pull request on GitHub, might invalidate or confuse discussions, and forces people who have local checkouts of the branch to re-checkout.

In case you want to clean up commits in a pull requests by rebasing, which changes the history, submit them as a new pull request and close the old one. Try to find the right point in time for this when it does not interfere with ongoing discussions or leave the cleanup to the final squash on merge. Reference the old pull request by adding something like `Supersedes #123` in the body of the new pull request.

## Who reviews

Anyone is free to comment on pull requests. Peer review and feedback are valuable.

Maintainers have a special role as they decide about merging the pull request.

If changes might have a critical impact, experts on the area of the change have to approve.

Maintainers and experts are documented in the MAINTAINERS.md file.

## How is review done

Review the code not the person. Be respectful, constructive, and supportive. Avoid judgement and ask questions instead.

Use the features of the GitHub review system. Comment on the code, propose changes, ask questions, and add a summary of your review.

### As a maintainer

If you approve the code, put an `ACK` into your comment and select the "Approve" option in the GitHub UI when submitting your review.

If you have reviewed the code but not tested it add an `utACK` into the comment. Depending on the impact of the change you have to decide if you add an approval so that the code can be merged or if you require another maintainer to do an approving review.

If you are fine with the concept of the change but haven't looked into the details add a `Concept ACK` in your comment. This is an indication for the author and other reviewers but doesn't approve the change. Another maintainer has to review and, if needed, test the change to approve it for merging.

If you disagree with the change add a `NACK` in a comment and provide your reasoning. That basically is a veto and has to be resolved before the code can be merged.

Nitpicking is fine as we do want to reach the highest quality possible. You can qualify nitpicks, which do not necessarily affect if a change can be merged or not, by adding a `Nit` in your comment.

There are special areas of the code which need expert review. As a maintainer it is your responsibility to involve others if you see that the change is touching these special areas. A definition of who is expert on which area of the code is in the MAINTAINERS.md file.

## Merging

Once approved, anybody with write access to the repo can merge the pull request. This can be the reviewer, the author, or somebody else.

Pull requests are squashed on merge to keep the history of the code clean and not pollute it with work in progress commits or artifacts of discussions in the pull request. The commit message should reflect the outcome of the discussion and describe what ends up in the commit being merged. The history of the pull request with its discussion can be looked up on GitHub.

Use the GitHub button for squashing and merging or do it on the command line using `git rebase -i`.

There are some exceptions to the rule to squash commits, for example when merging upstream code and the commits and authors should be kept as they are.

If the pull request was in a branch of the repository delete the branch once the pull request is merged. This keeps the repository clean and the history still is fully preserved on GitHub.

## Work in progress

Sometimes you want to get feedback before a change is done. Prepend the title of the pull request with `[WIP]`. That means that it must not be merged.

If you continue to work in the pull request and it gets to a state where it's ready to be merged, remove the `[WIP]` and notify reviewers that it's ready to be reviewed for merge.

## References

* [Designing awesome code reviews](https://medium.com/unpacking-trunk-club/designing-awesome-code-reviews-5a0d9cd867e3) (a great overview of how to do code reviews that are good for code and people)
* [Awesome code review](https://github.com/joho/awesome-code-review) (a curated list of resources related to code review)
