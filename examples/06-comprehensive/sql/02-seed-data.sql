-- 02-seed-data.sql: Sample data for demonstration

-- ============================================
-- Organizations
-- ============================================
INSERT INTO organizations (id, name, slug, plan, created_at) VALUES
    (1, 'Acme Corp', 'acme', 'enterprise', '2024-01-01'),
    (2, 'Startup Inc', 'startup', 'pro', '2024-02-15'),
    (3, 'Freelance Co', 'freelance', 'free', '2024-03-20');

-- ============================================
-- Users
-- ============================================
INSERT INTO users (id, org_id, email, name, role, status, created_at, last_login) VALUES
    -- Acme Corp users
    (1, 1, 'alice@acme.com', 'Alice Johnson', 'admin', 'active', '2024-01-01', '2024-06-28'),
    (2, 1, 'bob@acme.com', 'Bob Smith', 'member', 'active', '2024-01-15', '2024-06-27'),
    (3, 1, 'carol@acme.com', 'Carol Williams', 'member', 'active', '2024-02-01', '2024-06-28'),
    (4, 1, 'dave@acme.com', 'Dave Brown', 'viewer', 'active', '2024-03-01', '2024-06-20'),
    -- Startup Inc users
    (5, 2, 'eve@startup.io', 'Eve Davis', 'admin', 'active', '2024-02-15', '2024-06-28'),
    (6, 2, 'frank@startup.io', 'Frank Miller', 'member', 'active', '2024-03-01', '2024-06-25'),
    -- Freelance Co users
    (7, 3, 'grace@freelance.co', 'Grace Lee', 'admin', 'active', '2024-03-20', '2024-06-28');

-- ============================================
-- Projects
-- ============================================
INSERT INTO projects (id, org_id, name, description, status, owner_id, created_at) VALUES
    -- Acme Corp projects
    (1, 1, 'Website Redesign', 'Complete overhaul of company website', 'active', 1, '2024-01-10'),
    (2, 1, 'Mobile App v2', 'Next generation mobile application', 'active', 2, '2024-02-01'),
    (3, 1, 'API Migration', 'Migrate to new API infrastructure', 'active', 3, '2024-03-15'),
    -- Startup Inc projects
    (4, 2, 'MVP Launch', 'Initial product launch', 'active', 5, '2024-02-20'),
    (5, 2, 'Investor Deck', 'Series A pitch materials', 'archived', 5, '2024-03-01'),
    -- Freelance Co projects
    (6, 3, 'Client Portal', 'Self-service client portal', 'active', 7, '2024-04-01');

-- ============================================
-- Tasks
-- ============================================
INSERT INTO tasks (id, project_id, title, description, status, priority, assignee_id, due_date, created_at, completed_at) VALUES
    -- Website Redesign tasks
    (1, 1, 'Design mockups', 'Create initial design mockups', 'done', 'high', 1, '2024-02-01', '2024-01-15', '2024-01-28'),
    (2, 1, 'Frontend implementation', 'Build React components', 'in_progress', 'high', 2, '2024-03-15', '2024-02-01', NULL),
    (3, 1, 'Backend API updates', 'Update API endpoints', 'in_progress', 'medium', 3, '2024-03-20', '2024-02-15', NULL),
    (4, 1, 'QA testing', 'Full regression testing', 'todo', 'medium', 4, '2024-04-01', '2024-03-01', NULL),
    -- Mobile App tasks
    (5, 2, 'User research', 'Conduct user interviews', 'done', 'high', 2, '2024-02-15', '2024-02-01', '2024-02-14'),
    (6, 2, 'Prototype', 'Build clickable prototype', 'done', 'high', 1, '2024-03-01', '2024-02-15', '2024-02-28'),
    (7, 2, 'iOS development', 'Native iOS implementation', 'in_progress', 'high', 2, '2024-05-01', '2024-03-01', NULL),
    (8, 2, 'Android development', 'Native Android implementation', 'todo', 'high', 3, '2024-05-15', '2024-03-01', NULL),
    -- API Migration tasks
    (9, 3, 'Document current APIs', 'Create API documentation', 'done', 'medium', 3, '2024-04-01', '2024-03-15', '2024-03-28'),
    (10, 3, 'Design new schema', 'Design new API schema', 'review', 'high', 3, '2024-04-15', '2024-04-01', NULL),
    -- MVP Launch tasks
    (11, 4, 'Core features', 'Implement core features', 'in_progress', 'urgent', 5, '2024-04-01', '2024-02-20', NULL),
    (12, 4, 'Landing page', 'Create marketing landing page', 'done', 'high', 6, '2024-03-15', '2024-03-01', '2024-03-14'),
    (13, 4, 'Beta testing', 'Recruit and run beta test', 'todo', 'high', 5, '2024-05-01', '2024-03-20', NULL),
    -- Client Portal tasks
    (14, 6, 'Auth system', 'Implement authentication', 'done', 'high', 7, '2024-04-15', '2024-04-01', '2024-04-12'),
    (15, 6, 'Dashboard', 'Build main dashboard', 'in_progress', 'medium', 7, '2024-05-01', '2024-04-15', NULL);

-- ============================================
-- Activity Log
-- ============================================
INSERT INTO activity_log (id, org_id, user_id, action, entity_type, entity_id, created_at) VALUES
    (1, 1, 1, 'task.completed', 'task', 1, '2024-01-28 10:30:00'),
    (2, 1, 2, 'task.updated', 'task', 2, '2024-06-27 14:15:00'),
    (3, 1, 3, 'task.created', 'task', 10, '2024-04-01 09:00:00'),
    (4, 2, 5, 'project.created', 'project', 4, '2024-02-20 11:00:00'),
    (5, 2, 6, 'task.completed', 'task', 12, '2024-03-14 16:45:00'),
    (6, 3, 7, 'task.completed', 'task', 14, '2024-04-12 13:20:00');

-- ============================================
-- Daily Metrics
-- ============================================
INSERT INTO daily_metrics (date, org_id, active_users, tasks_created, tasks_completed) VALUES
    ('2024-06-25', 1, 3, 2, 1),
    ('2024-06-26', 1, 4, 1, 0),
    ('2024-06-27', 1, 3, 0, 2),
    ('2024-06-28', 1, 4, 1, 1),
    ('2024-06-25', 2, 2, 1, 0),
    ('2024-06-26', 2, 1, 0, 1),
    ('2024-06-27', 2, 2, 2, 0),
    ('2024-06-28', 2, 2, 0, 0),
    ('2024-06-25', 3, 1, 0, 1),
    ('2024-06-28', 3, 1, 1, 0);

.print '[02-seed-data] Sample data loaded'
