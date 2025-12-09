-- 03-views.sql: Views for common queries and analytics

-- ============================================
-- Organization Views
-- ============================================

-- Organization summary with counts
CREATE VIEW org_summary AS
SELECT
    o.id as org_id,
    o.name as org_name,
    o.slug,
    o.plan,
    COUNT(DISTINCT u.id) as user_count,
    COUNT(DISTINCT p.id) as project_count,
    COUNT(DISTINCT t.id) as task_count,
    o.created_at
FROM organizations o
LEFT JOIN users u ON o.id = u.org_id AND u.status = 'active'
LEFT JOIN projects p ON o.id = p.org_id AND p.status = 'active'
LEFT JOIN tasks t ON p.id = t.project_id
GROUP BY o.id, o.name, o.slug, o.plan, o.created_at;

-- ============================================
-- User Views
-- ============================================

-- User activity summary
CREATE VIEW user_summary AS
SELECT
    u.id as user_id,
    u.name,
    u.email,
    u.role,
    o.name as org_name,
    COUNT(DISTINCT t.id) FILTER (WHERE t.assignee_id = u.id) as assigned_tasks,
    COUNT(DISTINCT t.id) FILTER (WHERE t.assignee_id = u.id AND t.status = 'done') as completed_tasks,
    u.last_login
FROM users u
JOIN organizations o ON u.org_id = o.id
LEFT JOIN tasks t ON t.assignee_id = u.id
WHERE u.status = 'active'
GROUP BY u.id, u.name, u.email, u.role, o.name, u.last_login;

-- ============================================
-- Project Views
-- ============================================

-- Project health dashboard
CREATE VIEW project_health AS
SELECT
    p.id as project_id,
    p.name as project_name,
    o.name as org_name,
    p.status,
    owner.name as owner_name,
    COUNT(t.id) as total_tasks,
    COUNT(t.id) FILTER (WHERE t.status = 'done') as done_tasks,
    COUNT(t.id) FILTER (WHERE t.status = 'in_progress') as in_progress_tasks,
    COUNT(t.id) FILTER (WHERE t.status = 'todo') as todo_tasks,
    COUNT(t.id) FILTER (WHERE t.due_date < NOW()::TIMESTAMP::DATE AND t.status != 'done') as overdue_tasks,
    ROUND(100.0 * COUNT(t.id) FILTER (WHERE t.status = 'done') / NULLIF(COUNT(t.id), 0), 1) as completion_pct
FROM projects p
JOIN organizations o ON p.org_id = o.id
LEFT JOIN users owner ON p.owner_id = owner.id
LEFT JOIN tasks t ON p.id = t.project_id
WHERE p.status = 'active'
GROUP BY p.id, p.name, o.name, p.status, owner.name;

-- ============================================
-- Task Views
-- ============================================

-- Task board view (kanban style)
CREATE VIEW task_board AS
SELECT
    t.id as task_id,
    t.title,
    t.status,
    t.priority,
    p.name as project_name,
    assignee.name as assignee_name,
    t.due_date,
    CASE
        WHEN t.due_date < NOW()::TIMESTAMP::DATE AND t.status != 'done' THEN 'overdue'
        WHEN t.due_date = NOW()::TIMESTAMP::DATE THEN 'due_today'
        WHEN t.due_date <= NOW()::TIMESTAMP::DATE + INTERVAL '7 days' THEN 'due_soon'
        ELSE 'on_track'
    END as due_status
FROM tasks t
JOIN projects p ON t.project_id = p.id
LEFT JOIN users assignee ON t.assignee_id = assignee.id
WHERE p.status = 'active';

-- ============================================
-- Analytics Views
-- ============================================

-- Recent activity feed
CREATE VIEW recent_activity AS
SELECT
    a.id,
    a.action,
    a.entity_type,
    a.entity_id,
    u.name as user_name,
    o.name as org_name,
    a.created_at
FROM activity_log a
JOIN users u ON a.user_id = u.id
JOIN organizations o ON a.org_id = o.id
ORDER BY a.created_at DESC
LIMIT 100;

-- Weekly metrics summary
CREATE VIEW weekly_metrics AS
SELECT
    o.name as org_name,
    SUM(m.active_users) as total_active_user_days,
    SUM(m.tasks_created) as total_tasks_created,
    SUM(m.tasks_completed) as total_tasks_completed,
    ROUND(AVG(m.active_users), 1) as avg_daily_active_users
FROM daily_metrics m
JOIN organizations o ON m.org_id = o.id
WHERE m.date >= NOW()::TIMESTAMP::DATE - INTERVAL '7 days'
GROUP BY o.id, o.name;

