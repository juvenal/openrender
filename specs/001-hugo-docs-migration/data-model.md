# Data Model: Hugo Documentation Site

## Content Entities

### 1. Site Configuration (hugo.yaml)
- **Site Title**: Name of the project (Pixie)
- **Base URL**: GitHub Pages URL
- **Language**: Default language (en)
- **Theme**: Selected standard Hugo theme
- **Menu Configuration**: Navigation menu structure
- **Params**: Additional site-wide parameters
- **Privacy Settings**: Analytics, comments, etc.
- **Security Policy**: For link validation and accessibility

### 2. Landing Page (_index.md)
- **Title**: Site title
- **Description**: Brief overview of Pixie project
- **Content**: Welcome message, quick links to key sections
- **Front Matter**: 
  - title
  - description
  - weight (for navigation ordering)
  - layout (if custom layout needed)

### 3. User Manual Content
- **Installation Guide**
  - Title, content, prerequisites, steps
  - Front matter: title, weight, tags
- **Basic Usage**
  - Title, content, examples
  - Front matter: title, weight, tags
- **Advanced Topics**
  - Title, content, examples
  - Front matter: title, weight, tags

### 4. Developer Guide Content
- **Contributing Guidelines**
  - Title, content, prerequisites, steps
  - Front matter: title, weight, tags
- **Building from Source**
  - Title, content, prerequisites, steps
  - Front matter: title, weight, tags
- **Architecture Overview**
  - Title, content, diagrams, examples
  - Front matter: title, weight, tags

### 5. External References Content
- **RenderMan Specifications**
  - Title, content, external links
  - Front matter: title, weight, tags
- **Compatible Tools**
  - Title, content, external links
  - Front matter: title, weight, tags
- **Lighting Models**
  - Title, content, examples, external links
  - Front matter: title, weight, tags

### 6. Static Assets
- **Images**: Converted from `doc/images` and `doc/thumbs`
- **CSS**: Potentially minimal overrides if needed with standard theme
- **Icons**: Favicon and other icons

### 7. Navigation Structure
- **Main Menu**: Links to top-level sections (Home, Manual, Development, References)
- **Section Menus**: Sub-section navigation for each major section
- **Breadcrumbs**: Path identification for deep content
- **Sidebar Navigation**: Per-section navigation for related content

## Relationships
- Site Configuration defines global settings that affect all content
- Landing Page is the entry point to the documentation
- User Manual, Developer Guide, and External References are major content sections
- Each section contains multiple articles/pages
- Static assets are referenced by content pages
- Navigation structure provides pathways between related content