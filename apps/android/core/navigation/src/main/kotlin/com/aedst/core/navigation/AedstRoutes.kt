package com.aedst.core.navigation

/**
 * Well-known route paths every feature can navigate to without depending on
 * another feature's Gradle module — the navigation-contract equivalent of a
 * server module publishing a stable RPC method name instead of another module
 * importing its handler package directly.
 *
 * In-app links from the server (`Notification.link`, `SearchHit.link`, e.g.
 * `/tasks/<id>`) are these routes with a leading slash; [routeForLink] turns
 * one into a route after validating it.
 */
object AedstRoutes {
    const val AUTH_LOGIN = "auth/login"
    const val AUTH_ACCOUNT = "auth/account"

    const val TASKS = "tasks"
    const val TASK_ID_ARG = "taskId"
    const val TASK_DETAIL = "tasks/{$TASK_ID_ARG}"

    /** Separate prefix so it never collides with [TASK_DETAIL]'s `{taskId}` placeholder. */
    const val TASK_CREATE = "task-create"

    const val SEARCH = "search"

    const val NOTIFICATIONS = "notifications"
    const val NOTIFICATION_PREFERENCES = "notification-preferences"

    const val MEDIA = "media"
    const val MEDIA_ID_ARG = "mediaId"
    const val MEDIA_DETAIL = "media/{$MEDIA_ID_ARG}"

    /** Only registered for users with the `admin` role (see `SessionUser.isAdmin`). */
    const val ADMIN = "admin"

    /** Intent extra (and FCM `data` key) carrying a server in-app link such as `/tasks/<id>`. */
    const val EXTRA_LINK = "link"

    /** Signed-in landing destination. */
    const val HOME = TASKS

    fun taskDetail(taskId: String): String = "tasks/$taskId"

    fun mediaDetail(mediaId: String): String = "media/$mediaId"

    private val LINK_PREFIXES = listOf("tasks", "media", "notifications", "search", "admin")
    private val SAFE_SEGMENT = Regex("^[A-Za-z0-9_-]{1,64}$")

    /**
     * Maps a server-provided relative in-app link (`/tasks/<id>`) to a route, or `null` when it is not a link this
     * app understands. Absolute URLs, protocol-relative `//host` links, query strings and unexpected segments are
     * rejected, so a notification payload can never navigate to an arbitrary destination.
     */
    fun routeForLink(link: String?): String? {
        if (link.isNullOrBlank() || !link.startsWith("/") || link.startsWith("//")) return null
        val path = link.substringBefore('?').substringBefore('#').trim('/')
        val segments = path.split('/')
        if (segments.isEmpty() || segments.size > 2 || segments.first() !in LINK_PREFIXES) return null
        if (!segments.all(SAFE_SEGMENT::matches)) return null
        return path
    }
}
