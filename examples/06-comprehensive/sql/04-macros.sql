-- 04-macros.sql: Custom macros that serve as domain-specific tools

-- ============================================
-- Organization Tools
-- ============================================

-- Organization dashboard: Get comprehensive org metrics
CREATE MACRO org_dashboard(org_id_param) AS TABLE
    SELECT
        o.name as organization,
        o.plan,
        o.created_at as member_since,
        (SELECT COUNT(*) FROM users u WHERE u.org_id = o.id AND u.status = 'active') as active_users,
        (SELECT COUNT(*) FROM projects p WHERE p.org_id = o.id AND p.status = 'active') as active_projects,
        (SELECT COUNT(*) FROM tasks t
         JOIN projects p ON t.project_id = p.id
         WHERE p.org_id = o.id AND t.status != 'done') as open_tasks,
        (SELECT COUNT(*) FROM tasks t
         JOIN projects p ON t.project_id = p.id
         WHERE p.org_id = o.id AND t.status = 'done') as completed_tasks,
        (SELECT COUNT(*) FROM tasks t
         JOIN projects p ON t.project_id = p.id
         WHERE p.org_id = o.id AND t.due_date < CURRENT_DATE AND t.status != 'done') as overdue_tasks
    FROM organizations o
    WHERE o.id = org_id_param;

-- ============================================
-- User Tools
-- ============================================

-- User activity report: Get detailed user activity
CREATE MACRO user_activity(user_id_param) AS TABLE
    SELECT
        u.name,
        u.email,
        u.role,
        o.name as organization,
        u.last_login,
        (SELECT COUNT(*) FROM tasks t WHERE t.assignee_id = u.id AND t.status != 'done') as pending_tasks,
        (SELECT COUNT(*) FROM tasks t WHERE t.assignee_id = u.id AND t.status = 'done') as completed_tasks,
        (SELECT COUNT(*) FROM tasks t WHERE t.assignee_id = u.id AND t.due_date < CURRENT_DATE AND t.status != 'done') as overdue_tasks,
        (SELECT COUNT(*) FROM activity_log a WHERE a.user_id = u.id AND a.created_at >= CURRENT_DATE - INTERVAL '7 days') as actions_this_week
    FROM users u
    JOIN organizations o ON u.org_id = o.id
    WHERE u.id = user_id_param;

-- User tasks: Get all tasks for a user with optional status filter
CREATE MACRO user_tasks(user_id_param, status_filter) AS TABLE
    SELECT
        t.id,
        t.title,
        t.status,
        t.priority,
        p.name as project,
        t.due_date,
        CASE
            WHEN t.due_date < CURRENT_DATE AND t.status != 'done' THEN 'OVERDUE'
            WHEN t.due_date = CURRENT_DATE THEN 'DUE TODAY'
            ELSE 'OK'
        END as due_status
    FROM tasks t
    JOIN projects p ON t.project_id = p.id
    WHERE t.assignee_id = user_id_param
      AND (status_filter IS NULL OR t.status = status_filter)
    ORDER BY
        CASE t.priority
            WHEN 'urgent' THEN 1
            WHEN 'high' THEN 2
            WHEN 'medium' THEN 3
            WHEN 'low' THEN 4
        END,
        t.due_date NULLS LAST;

-- ============================================
-- Project Tools
-- ============================================

-- Project status: Get detailed project status
CREATE MACRO project_status(project_id_param) AS TABLE
    SELECT
        p.name as project,
        p.description,
        p.status,
        owner.name as owner,
        o.name as organization,
        p.created_at,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id) as total_tasks,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id AND t.status = 'done') as done,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id AND t.status = 'in_progress') as in_progress,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id AND t.status = 'review') as in_review,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id AND t.status = 'todo') as todo,
        (SELECT COUNT(*) FROM tasks t WHERE t.project_id = p.id AND t.due_date < CURRENT_DATE AND t.status != 'done') as overdue
    FROM projects p
    JOIN organizations o ON p.org_id = o.id
    LEFT JOIN users owner ON p.owner_id = owner.id
    WHERE p.id = project_id_param;

-- ============================================
-- Task Tools
-- ============================================

-- Task search: Search tasks with multiple filters
CREATE MACRO task_search(search_term, status_filter, priority_filter, project_id_filter) AS TABLE
    SELECT
        t.id,
        t.title,
        t.description,
        t.status,
        t.priority,
        p.name as project,
        assignee.name as assignee,
        t.due_date,
        t.created_at
    FROM tasks t
    JOIN projects p ON t.project_id = p.id
    LEFT JOIN users assignee ON t.assignee_id = assignee.id
    WHERE (search_term IS NULL OR t.title ILIKE '%' || search_term || '%' OR t.description ILIKE '%' || search_term || '%')
      AND (status_filter IS NULL OR t.status = status_filter)
      AND (priority_filter IS NULL OR t.priority = priority_filter)
      AND (project_id_filter IS NULL OR t.project_id = project_id_filter)
    ORDER BY t.created_at DESC;

-- Overdue tasks: Get all overdue tasks across projects
CREATE MACRO overdue_tasks(org_id_param) AS TABLE
    SELECT
        t.id,
        t.title,
        t.priority,
        p.name as project,
        assignee.name as assignee,
        t.due_date,
        CURRENT_DATE - t.due_date as days_overdue
    FROM tasks t
    JOIN projects p ON t.project_id = p.id
    LEFT JOIN users assignee ON t.assignee_id = assignee.id
    WHERE p.org_id = org_id_param
      AND t.due_date < CURRENT_DATE
      AND t.status != 'done'
    ORDER BY t.due_date ASC;

-- ============================================
-- Analytics Tools
-- ============================================

-- Productivity metrics: Get productivity metrics for date range
CREATE MACRO productivity_metrics(org_id_param, start_date, end_date) AS TABLE
    SELECT
        m.date,
        m.active_users,
        m.tasks_created,
        m.tasks_completed,
        ROUND(100.0 * m.tasks_completed / NULLIF(m.tasks_created, 0), 1) as completion_rate
    FROM daily_metrics m
    WHERE m.org_id = org_id_param
      AND m.date >= start_date
      AND m.date <= end_date
    ORDER BY m.date;

.print '[04-macros] Custom macros created'
