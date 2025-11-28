-- 01-schema.sql: Database schema for a multi-tenant SaaS application

-- ============================================
-- Core Entities
-- ============================================

-- Organizations (tenants)
CREATE TABLE organizations (
    id INTEGER PRIMARY KEY,
    name VARCHAR NOT NULL,
    slug VARCHAR UNIQUE NOT NULL,
    plan VARCHAR DEFAULT 'free',  -- free, pro, enterprise
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    settings JSON DEFAULT '{}'
);

-- Users
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    org_id INTEGER REFERENCES organizations(id),
    email VARCHAR NOT NULL,
    name VARCHAR NOT NULL,
    role VARCHAR DEFAULT 'member',  -- admin, member, viewer
    status VARCHAR DEFAULT 'active',  -- active, invited, suspended
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    last_login TIMESTAMP,
    UNIQUE(org_id, email)
);

-- Projects
CREATE TABLE projects (
    id INTEGER PRIMARY KEY,
    org_id INTEGER REFERENCES organizations(id),
    name VARCHAR NOT NULL,
    description TEXT,
    status VARCHAR DEFAULT 'active',  -- active, archived, deleted
    owner_id INTEGER REFERENCES users(id),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Tasks
CREATE TABLE tasks (
    id INTEGER PRIMARY KEY,
    project_id INTEGER REFERENCES projects(id),
    title VARCHAR NOT NULL,
    description TEXT,
    status VARCHAR DEFAULT 'todo',  -- todo, in_progress, review, done
    priority VARCHAR DEFAULT 'medium',  -- low, medium, high, urgent
    assignee_id INTEGER REFERENCES users(id),
    due_date DATE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    completed_at TIMESTAMP
);

-- ============================================
-- Activity & Analytics
-- ============================================

-- Activity log
CREATE TABLE activity_log (
    id INTEGER PRIMARY KEY,
    org_id INTEGER REFERENCES organizations(id),
    user_id INTEGER REFERENCES users(id),
    action VARCHAR NOT NULL,
    entity_type VARCHAR,
    entity_id INTEGER,
    metadata JSON,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Daily metrics (pre-aggregated)
CREATE TABLE daily_metrics (
    date DATE,
    org_id INTEGER REFERENCES organizations(id),
    active_users INTEGER DEFAULT 0,
    tasks_created INTEGER DEFAULT 0,
    tasks_completed INTEGER DEFAULT 0,
    PRIMARY KEY (date, org_id)
);

.print '[01-schema] Database schema created'
