---
title: "Markdown Formatting Guidelines"
date: 2025-12-08
---

# Markdown Formatting Guidelines

This document provides guidelines for formatting content in the Pixie documentation using Markdown.

## Heading Structure

Use ATX-style headings with a space after the hash symbol:

```markdown
# Main Heading (H1) - Only use once per page
## Section Heading (H2)
### Subsection Heading (H3)
#### Further Subdivision (H4)
```

## Text Formatting

Use these elements to emphasize text appropriately:

- **Bold text**: `**Bold text**`
- *Italic text*: `*Italic text*`
- ~~Strikethrough~~: `~~Strikethrough~~`

## Lists

### Unordered Lists
Use hyphens, asterisks, or plus signs for unordered lists:

```markdown
- Item 1
- Item 2
  - Nested item
```

### Ordered Lists
Use numbers followed by periods for ordered lists:

```markdown
1. First item
2. Second item
   1. Nested item
```

## Code Blocks

Use triple backticks for code blocks with optional syntax highlighting:

````markdown
```cpp
#include <iostream>
int main() {
    std::cout << "Hello World" << std::endl;
    return 0;
}
```
````

For inline code, use single backticks: `code here`

## Links

Use the following format for internal links:

```markdown
[Link text](/pixie-code/path/to/page/)
```

For external links:

```markdown
[Link text](https://external-site.com)
```

## Images

Use the following format for images:

```markdown
![Alt text](/pixie-code/static/path/to/image.jpg)
```

Always include descriptive alt text for accessibility.

## Tables

Format tables like this:

```markdown
| Column 1 | Column 2 | Column 3 |
|----------|----------|----------|
| Cell 1   | Cell 2   | Cell 3   |
| Cell 4   | Cell 5   | Cell 6   |
```

## Blockquotes

Use the `>` symbol for blockquotes:

```markdown
> This is a blockquote
> It can span multiple lines
```

## Horizontal Rules

Use three hyphens for horizontal rules:

```markdown
---
```

## Hugo-Specific Features

### Shortcodes

Use Hugo shortcodes when available for special functionality:

```markdown
{{</* shortcode-name */>}}
```

### Internal Linking

Use Hugo's internal linking features when possible:

```markdown
[[Page Title]]
```

## Best Practices

- Keep lines under 80 characters when possible
- Use blank lines to separate paragraphs and elements
- Maintain consistent indentation
- Use descriptive alt text for images
- Write link text that makes sense out of context
- Use appropriate heading hierarchy (don't skip levels)