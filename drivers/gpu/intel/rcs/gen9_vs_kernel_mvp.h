// gen9_vs_kernel_mvp.h
// Extracted via INTEL_DEBUG=vs,fs,nocompact,hex from cube_mvp_extract.c
// (uniform mat4 mvp; gl_Position = mvp * vec4(pos, 1.0); vColor = aColor;)


static const u32 gen9_vs_kernel_mvp[] = {
    // mul(8)  g19 = g2.4 * g5     (col1.x * pos.y)
    0x00600041u, 0x22603ae8u, 0x3a000050u, 0x008d00a0u,
    // mul(8)  g20 = g2.5 * g5     (col1.y * pos.y)
    0x00600041u, 0x22803ae8u, 0x3a000054u, 0x008d00a0u,
    // mul(8)  g21 = g2.6 * g5     (col1.z * pos.y)
    0x00600041u, 0x22a03ae8u, 0x3a000058u, 0x008d00a0u,
    // mul(8)  g22 = g2.7 * g5     (col1.w * pos.y)
    0x00600041u, 0x22c03ae8u, 0x3a00005cu, 0x008d00a0u,
    // mov(8)  g11 = 0D            (URB output slot 0: PointSize = 0)
    0x00600001u, 0x21600e28u, 0x08000000u, 0x00000000u,
    // mov(8)  g12 = 0D
    0x00600001u, 0x21800e28u, 0x08000000u, 0x00000000u,
    // mov(8)  g13 = 0D
    0x00600001u, 0x21a00e28u, 0x08000000u, 0x00000000u,
    // mov(8)  g14 = 0D
    0x00600001u, 0x21c00e28u, 0x08000000u, 0x00000000u,
    // mad(8)  g23 = g19 + g4*g2.0   (+ col0.x * pos.x)     { align16 }
    0x0060015bu, 0x171e0000u, 0x390131c8u, 0x00800408u,
    // mad(8)  g24 = g20 + g4*g2.1                          { align16 }
    0x0060015bu, 0x181e0000u, 0x390141c8u, 0x00880408u,
    // mad(8)  g25 = g21 + g4*g2.2                          { align16 }
    0x0060015bu, 0x191e0000u, 0x390151c8u, 0x00900408u,
    // mad(8)  g26 = g22 + g4*g2.3                          { align16 }
    0x0060015bu, 0x1a1e0000u, 0x390161c8u, 0x00980408u,
    // mad(8)  g27 = g23 + g6*g3.0   (+ col2.x * pos.z)     { align16 }
    0x0060015bu, 0x1b1e0000u, 0x390171c8u, 0x00c0040cu,
    // mad(8)  g28 = g24 + g6*g3.1                          { align16 }
    0x0060015bu, 0x1c1e0000u, 0x390181c8u, 0x00c8040cu,
    // mad(8)  g29 = g25 + g6*g3.2                          { align16 }
    0x0060015bu, 0x1d1e0000u, 0x390191c8u, 0x00d0040cu,
    // mad(8)  g30 = g26 + g6*g3.3                          { align16 }
    0x0060015bu, 0x1e1e0000u, 0x3901a1c8u, 0x00d8040cu,
    // add(8)  g15 = g27 + g3.4      (+ col3.x, w=1 term)
    0x00600040u, 0x21e03ae8u, 0x3a8d0360u, 0x00000070u,
    // add(8)  g16 = g28 + g3.5
    0x00600040u, 0x22003ae8u, 0x3a8d0380u, 0x00000074u,
    // add(8)  g17 = g29 + g3.6
    0x00600040u, 0x22203ae8u, 0x3a8d03a0u, 0x00000078u,
    // add(8)  g18 = g30 + g3.7
    0x00600040u, 0x22403ae8u, 0x3a8d03c0u, 0x0000007cu,
    // sends(8) null g1 g11   URB write offset=0 SIMD8 mlen=1 ex_mlen=8 rlen=0
    0x06600033u, 0x0000b010u, 0x00000028u, 0x02080007u,
    // mov(8)  g122 = g8    (color/varying passthrough into 2nd URB write)
    0x00600001u, 0x2f400208u, 0x008d0100u, 0x00000000u,
    // mov(8)  g123 = g9
    0x00600001u, 0x2f600208u, 0x008d0120u, 0x00000000u,
    // mov(8)  g124 = g10
    0x00600001u, 0x2f800208u, 0x008d0140u, 0x00000000u,
    // mov(8)  g126 = g1    { WE_all } (header for 2nd send)
    0x00600001u, 0x2fc0020cu, 0x008d0020u, 0x00000000u,
    // sends(8) null g126 g122  URB write offset=2 SIMD8 mlen=1 ex_mlen=4 rlen=0 EOT
    0x06600033u, 0x0007a010u, 0x00000fc4u, 0x82080027u,
};
static const usize gen9_vs_kernel_mvp_size = sizeof(gen9_vs_kernel_mvp);
