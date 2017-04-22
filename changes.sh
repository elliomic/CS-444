#!/bin/bash
# Find remote URL for hashes (designed for GitHub-hosted projects)
origin=`git config remote.origin.url`
base=`dirname "$origin"`/`basename "$origin" .git`

# Output LaTeX table in chronological order
echo "\\begin{tabular}{l l l}\\textbf{Detail} & \\textbf{Author} & \\textbf{Description}\\\\\\hline"
git log --pretty=format:"\\href{$base/commit/%H}{%h} & %an & %s\\\\\\hline" --reverse
echo "\end{tabular}"

# Git log to tex script, courtesy:
# https://mike42.me/blog/2014-04-including-git-commit-history-in-a-latex-document