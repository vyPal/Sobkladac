# AI Use

AI use was not limited on a project level more than it is limited by the project specification.

AI use disclosure was left to each member to disclose themselves.

## AI Use in Pull Request Reviews

At the beginning of the project, a custom AI agent (utilizing the Claude model family) was deployed to provide summaries of open PRs and had the ability to find and describe potential errors/bugs/mistakes on command to assist a human reviewer. These reviews were found early on to contain a lot of false positives or brought up small meaningless issues, so this agent wasn't used later

## Format for AI-generated or AI-assisted code

Team members were obliged to disclose AI use in code they submitted, the following text is copied from the repository's README.md file which was used as internal documentation/instructions. Code written by AI can be found by searching the files for lines containing `AI` or `AI-BEGIN`

Code that was generated or modified by AI, or who's creation was greatly
influenced by AI has to be denoted in the following way:
(`<>` - marks a required section, `()` - marks an optional section)

```
// Multi-line:

// AI-BEGIN [<ai model> (, <tool through which the model was used>)]
// Prompt: "<prompt>"
// Context: "(additional context available to the AI)"
code...
// AI-END

// Single line:

code... // AI [<model>]: "<prompt>"
```
