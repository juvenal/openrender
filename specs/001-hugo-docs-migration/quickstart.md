# Quickstart Guide: Hugo Documentation Site

## Prerequisites
- Hugo static site generator (v0.120 or higher)
- Git
- GitHub account for deployment

## Setting up the Hugo Site

1. **Install Hugo**:
   ```bash
   # On macOS using Homebrew
   brew install hugo
   
   # On other platforms, see https://gohugo.io/getting-started/installing/
   ```

2. **Navigate to the site directory**:
   ```bash
   cd docs/site
   ```

3. **Initialize the Hugo site** (if not already done):
   ```bash
   hugo new site .
   ```

4. **Add a theme** (using a standard documentation theme):
   ```bash
   # Add theme as a git submodule
   git init  # if not already a git repo
   git submodule add https://github.com/example/docsy-theme.git themes/docsy
   ```

5. **Update the config file**:
   - Ensure `hugo.yaml` points to the correct theme
   - Configure site metadata, menus, and parameters

## Running the Site Locally

1. **Start the Hugo server**:
   ```bash
   hugo server
   ```

2. **View the site**:
   - Open your browser to http://localhost:1313
   - The site will automatically reload when content changes

## Adding Content

1. **Create a new content file**:
   ```bash
   hugo new content/manual/new-topic.md
   ```

2. **Edit the content** in the appropriate section under `content/`
   - Use Markdown format
   - Include proper front matter at the top of each file

3. **Add images** to the `static/images/` directory
   - Reference them in content as `/images/filename.jpg`

## Building for Production

1. **Generate static files**:
   ```bash
   hugo
   ```

2. **Output** will be in the `public/` directory
   - Deploy this directory to GitHub Pages

## Deploying to GitHub Pages

The GitHub Actions workflow (`.github/workflows/docs-deploy.yml`) will automatically:
1. Build the Hugo site when changes are pushed to main branch
2. Deploy the generated files to GitHub Pages
3. Validate link integrity and site functionality

## Content Structure Guidelines

- `content/_index.md`: Landing page
- `content/manual/`: User manual and guides
- `content/development/`: Developer documentation
- `content/references/`: External references and links
- `static/images/`: Static assets (images, icons)
- `hugo.yaml`: Configuration file

## Troubleshooting

- If the site doesn't load properly, check that the theme is properly configured in `hugo.yaml`
- For broken links, run the link validation script mentioned in the workflow
- For content issues, ensure Markdown syntax is correct