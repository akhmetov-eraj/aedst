-- name: UpsertProfile :one
INSERT INTO profile.profiles (user_id, display_name, bio, avatar_url, updated_at)
VALUES ($1, $2, $3, $4, $5)
ON CONFLICT (user_id) DO UPDATE
SET display_name = EXCLUDED.display_name,
    bio = EXCLUDED.bio,
    avatar_url = EXCLUDED.avatar_url,
    updated_at = EXCLUDED.updated_at
RETURNING user_id, display_name, bio, avatar_url, updated_at;

-- name: FindProfileByUserID :one
SELECT user_id, display_name, bio, avatar_url, updated_at
FROM profile.profiles
WHERE user_id = $1;
