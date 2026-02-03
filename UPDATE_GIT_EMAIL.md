# Git Email Update Procedure

This guide walks through updating the git email for all commits in a repository's history.

## Prerequisites

**⚠️ Important Warnings:**
- This rewrites git history - all commit hashes will change
- Only do this if no one else has cloned the repository
- If you've already done this in other repos, make sure you're in the correct repository before proceeding

## Step 0: Check Current State

First, verify what email(s) are currently in the commit history:

```bash
git log --format="%an <%ae>" | sort -u
```

Check your current local git config:

```bash
git config user.email
git config user.name
```

**Record the OLD_EMAIL** from the commit history - you'll need it for Step 2.

## Step 1: Update Local Git Config

Update your local git configuration to use the new email for future commits:

```bash
git config user.email "ryan_devens@proton.me"
```

**Verify:**
```bash
git config user.email
```

Should output: `ryan_devens@proton.me`

**✓ Checkpoint:** Confirm the email is correct before proceeding.

---

## Step 2: Rewrite Commit History

**⚠️ This step rewrites all commit history!**

Replace `OLD_EMAIL` with the email you found in Step 0:

```bash
git filter-branch --env-filter '
OLD_EMAIL="your-old-email@example.com"
NEW_EMAIL="ryan_devens@proton.me"
NEW_NAME="Ryan Devens"

if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]
then
    export GIT_COMMITTER_EMAIL="$NEW_EMAIL"
    export GIT_COMMITTER_NAME="$NEW_NAME"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]
then
    export GIT_AUTHOR_EMAIL="$NEW_EMAIL"
    export GIT_AUTHOR_NAME="$NEW_NAME"
fi
' --tag-name-filter cat -- --branches --tags
```

You'll see a warning about git-filter-branch - this is normal. The command will proceed automatically.

**Verify:**
```bash
git log --format="%an <%ae>" | sort -u
```

Should output: `Ryan Devens <ryan_devens@proton.me>`

**✓ Checkpoint:** Confirm all commits now show the new email before proceeding.

---

## Step 3: Force Push to Remote

Push the rewritten history to the remote repository:

```bash
git push --force-with-lease origin main
```

**Note:** If your default branch is not `main`, replace with the correct branch name (e.g., `master`).

**Verify:**

Visit your GitHub repository and check a recent commit to confirm the email has been updated on the remote.

---

## Success Checklist

- [ ] Local git config shows new email
- [ ] All commits in local history show new email
- [ ] Remote repository shows updated commits with new email

---

## Troubleshooting

**Multiple emails in commit history:**
If Step 0 shows multiple emails, you may need to run Step 2 multiple times with different OLD_EMAIL values, or modify the script to handle multiple old emails.

**Branch name is not 'main':**
Replace `main` with your actual branch name in Step 3. Check with:
```bash
git branch --show-current
```

**filter-branch already run warning:**
If you see "Cannot create a new backup", you may have already run filter-branch. Either:
1. Clean up with: `git update-ref -d refs/original/refs/heads/main`
2. Or add `-f` flag: `git filter-branch -f --env-filter ...`

---

## Additional Repositories

When repeating this procedure in other repositories:

1. Navigate to the repository: `cd /path/to/repository`
2. Verify you're in the correct repo: `git remote -v`
3. Follow Steps 0-3 above
4. The OLD_EMAIL may be different in each repository - always check Step 0 first!
