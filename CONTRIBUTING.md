# Syncing
 
If a shared branch has been rebased upstream, **do not use `git pull`** — it will create a messy merge commit.
 
### Updating your local `main`
 
```bash
git switch main
git fetch origin main
git reset --hard origin/main
```
 
### Rebasing your branch on top of updated `main`
 
```bash
git switch <branch-name>
git rebase main
git push --force-with-lease origin <branch-name>
```
 
### If someone else force-pushed a shared branch
 
```bash
git fetch origin
git reset --hard origin/<branch-name>
```
 
> ⚠️ `--force-with-lease` is safer than `-f` — it will refuse to push if someone else has pushed to the branch since your last fetch, preventing accidental overwrites.
>
> ⚠️ `reset --hard` discards local unpushed commits. Make sure you have no uncommitted work before running it.
