/**
 * @file water.js
 * @brief Dynamic procedural water surface mesh and wave shader for Three.js
 */

class WaterSurface {
    constructor(scene, size = 160, segments = 80) {
        this.scene = scene;
        this.size = size;
        this.segments = segments;
        this.mesh = null;
        this.material = null;
        this.init();
    }

    init() {
        const geometry = new THREE.PlaneGeometry(this.size, this.size, this.segments, this.segments);
        geometry.rotateX(-Math.PI / 2);

        // Custom Vertex and Fragment Shaders for undulating water waves
        const customMaterial = new THREE.ShaderMaterial({
            uniforms: {
                uTime: { value: 0.0 },
                uWaterColor: { value: new THREE.Color(0x06395b) },
                uWaterDeep: { value: new THREE.Color(0x021626) },
                uFoamColor: { value: new THREE.Color(0x73d2ff) },
                uLightPosition: { value: new THREE.Vector3(40, 60, 40) }
            },
            vertexShader: `
                uniform float uTime;
                varying vec3 vWorldPosition;
                varying vec3 vNormal;
                varying float vWaveHeight;

                // Wave function
                float getWave(vec2 pos, float time) {
                    float wave = sin(pos.x * 0.15 + time * 1.5) * 0.45;
                    wave += cos(pos.y * 0.18 + time * 1.2) * 0.35;
                    wave += sin((pos.x + pos.y) * 0.25 + time * 2.0) * 0.2;
                    return wave;
                }

                void main() {
                    vec3 pos = position;
                    float wave = getWave(pos.xz, uTime);
                    pos.y += wave;
                    vWaveHeight = wave;

                    // Compute approximate normal for specular highlights
                    float delta = 0.1;
                    float waveX = getWave(pos.xz + vec2(delta, 0.0), uTime);
                    float waveZ = getWave(pos.xz + vec2(0.0, delta), uTime);
                    vec3 norm = normalize(vec3(wave - waveX, delta, wave - waveZ));
                    vNormal = normalMatrix * norm;

                    vec4 worldPos = modelMatrix * vec4(pos, 1.0);
                    vWorldPosition = worldPos.xyz;
                    gl_Position = projectionMatrix * viewMatrix * worldPos;
                }
            `,
            fragmentShader: `
                uniform vec3 uWaterColor;
                uniform vec3 uWaterDeep;
                uniform vec3 uFoamColor;
                uniform vec3 uLightPosition;
                varying vec3 vWorldPosition;
                varying vec3 vNormal;
                varying float vWaveHeight;

                void main() {
                    vec3 lightDir = normalize(uLightPosition - vWorldPosition);
                    vec3 viewDir = normalize(cameraPosition - vWorldPosition);
                    vec3 normal = normalize(vNormal);

                    // Diffuse
                    float diff = max(dot(normal, lightDir), 0.0);

                    // Specular glint
                    vec3 reflectDir = reflect(-lightDir, normal);
                    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

                    // Fresnel effect
                    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 2.0);

                    // Depth & crest color gradient
                    vec3 baseCol = mix(uWaterDeep, uWaterColor, clamp(vWaveHeight + 0.5, 0.0, 1.0));
                    baseCol = mix(baseCol, uFoamColor, fresnel * 0.4 + spec * 0.6);

                    gl_FragColor = vec4(baseCol + vec3(spec * 0.7), 0.88);
                }
            `,
            transparent: true,
            side: THREE.DoubleSide
        });

        this.material = customMaterial;
        this.mesh = new THREE.Mesh(geometry, customMaterial);
        this.mesh.position.y = 0;
        this.mesh.receiveShadow = true;
        this.scene.add(this.mesh);
    }

    update(elapsedSeconds) {
        if (this.material && this.material.uniforms) {
            this.material.uniforms.uTime.value = elapsedSeconds;
        }
    }

    getWaterHeightAt(x, z, time) {
        return Math.sin(x * 0.15 + time * 1.5) * 0.45 +
               Math.cos(z * 0.18 + time * 1.2) * 0.35 +
               Math.sin((x + z) * 0.25 + time * 2.0) * 0.2;
    }
}

window.WaterSurface = WaterSurface;
