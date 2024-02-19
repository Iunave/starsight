// Returns ±1
vec2 sign_not_zero(vec2 v)
{
    return vec2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
}

vec2 octEncode(vec3 v)
{
    //Project the sphere onto the octahedron, and then onto the xy plane
    vec2 p = v.xy * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
    // Reflect the folds of the lower hemisphere over the diagonals
    return (v.z <= 0.0) ? ((1.0 - abs(p.yx)) * sign_not_zero(p)) : p;
}

vec3 octDecode(vec2 e)
{
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);
    return normalize(v);
}

vec3 quatRotateVec(vec4 q, vec3 v)
{
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

uint64_t padSize2Alignment(uint64_t size, uint64_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}