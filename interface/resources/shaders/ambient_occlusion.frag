#version 120

//
//  ambient_occlusion.frag
//  fragment shader
//
//  Created by Andrzej Kapolka on 7/5/13.
//  Copyright (c) 2013 High Fidelity, Inc. All rights reserved.
//

// the depth texture
uniform sampler2D depthTexture;

// the random rotation texture
uniform sampler2D rotationTexture;

// the sample kernel containing the unit offset vectors
const int SAMPLE_KERNEL_SIZE = 16;
uniform vec3 sampleKernel[SAMPLE_KERNEL_SIZE];

// the distance to the near clip plane
uniform float near;

// the distance to the far clip plane
uniform float far;

// the left and bottom edges of the view window
uniform vec2 leftBottom;

// the right and top edges of the view window
uniform vec2 rightTop;

// the radius of the effect
uniform float radius;

// the scale for the noise texture
uniform vec2 noiseScale;

// given a texture coordinate, returns the 3D view space z coordinate
float texCoordToViewSpaceZ(vec2 texCoord) {
    return (far * near) / (texture2D(depthTexture, texCoord).r * (far - near) - far);
}

// given a texture coordinate, returns the 3D view space coordinate
vec3 texCoordToViewSpace(vec2 texCoord) {
    float z = texCoordToViewSpaceZ(texCoord);
    return vec3((leftBottom + texCoord * (rightTop - leftBottom)) * (-z / near), z);
}

void main(void) {
    vec3 rotationX = texture2D(rotationTexture, gl_TexCoord[0].st * noiseScale).rgb;
    vec3 rotationY = normalize(cross(rotationX, vec3(0.0, 0.0, 1.0)));
    mat3 rotation = mat3(rotationX, rotationY, cross(rotationX, rotationY));
    
    vec3 center = texCoordToViewSpace(gl_TexCoord[0].st);
    
    float occlusion = 4.0;
    for (int i = 0; i < SAMPLE_KERNEL_SIZE; i++) {
        vec3 offset = center + rotation * (radius * sampleKernel[i]);
        vec4 projected = gl_ProjectionMatrix * vec4(offset, 1.0);
        float depth = texCoordToViewSpaceZ(projected.xy * 0.5 / projected.w + vec2(0.5, 0.5));
        occlusion += 1.0 - step(offset.z, depth);
    }
    
    gl_FragColor = vec4(occlusion, occlusion, occlusion, 0.0) / 16.0;
}
