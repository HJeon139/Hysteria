Make sure when you clone this to your desktop that you CLICK ON CLONE TO and place it in C:\Program Files (x86)\Steam\SteamApps\sourcemods\

Refer to this when closing using git shell: http://git-scm.com/docs/git-clone

Things to check whenever you pull:
1) does it compile?
2) does it run from steam?

Things to check before you push:
1) does it compile?
2) does it run from steam?

git shell quick reference:{

git add .			\\scans for new stuff to commit
git commit -m 'message'		\\commits changes with the message 'message'
git push -u origin master	\\pushes to master using -u option that uses Delta Compression and is a lot faster to upload
git pull			\\downloads commits by others. ALLWAYS PULL BEFORE PUSH!

}