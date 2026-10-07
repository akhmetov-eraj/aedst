package com.aedst.feature.auth.data.local

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.contentOrNull
import java.util.Base64

/**
 * Reads the role claim from an access token's payload **for UI gating only** (show/hide the admin area). The
 * signature is deliberately not verified here: the client cannot verify it meaningfully, and every protected RPC is
 * authorised by the server (`AEDST_ROLE_POLICY`), so a forged claim only reveals screens whose calls then fail with
 * PERMISSION_DENIED.
 *
 * Mirrors the server's `rolesFromToken`: [ROLES_CLAIM_PATHS] may be a top-level claim (`roles`, the first-party
 * issuer) or a dotted path (Keycloak's `realm_access.roles`); the value may be an array of strings or a
 * space/comma-separated string.
 */
internal object AccessTokenClaims {
    private val ROLES_CLAIM_PATHS = listOf("roles", "realm_access.roles")
    private const val JWT_PARTS = 3

    fun roles(accessToken: String): Set<String> {
        val payload = payload(accessToken) ?: return emptySet()
        return ROLES_CLAIM_PATHS.asSequence()
            .mapNotNull { path -> payload.at(path) }
            .map(::rolesOf)
            .firstOrNull { it.isNotEmpty() }
            .orEmpty()
    }

    private fun payload(token: String): JsonObject? {
        val parts = token.split('.')
        if (parts.size != JWT_PARTS) return null
        return runCatching {
            val json = String(Base64.getUrlDecoder().decode(parts[1].trimEnd('=')), Charsets.UTF_8)
            Json.parseToJsonElement(json) as? JsonObject
        }.getOrNull()
    }

    private fun JsonObject.at(path: String): JsonElement? =
        path.split('.').fold(this as JsonElement?) { node, key -> (node as? JsonObject)?.get(key) }

    private fun rolesOf(element: JsonElement): Set<String> = when (element) {
        is JsonArray -> element.mapNotNull { (it as? JsonPrimitive)?.contentOrNull }.toSet()
        is JsonPrimitive -> element.contentOrNull.orEmpty().split(' ', ',').filter(String::isNotBlank).toSet()
        else -> emptySet()
    }
}
