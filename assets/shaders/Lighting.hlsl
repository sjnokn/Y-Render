float3 EvaluateDirectionalLight(float3 normal, float3 viewDir, float3 lightDirection, float3 lightColor, float intensity, float specularPower)
{
    float3 lightDir = normalize(-lightDirection);
    float3 halfDir = normalize(lightDir + viewDir);
    float ndotl = saturate(dot(normal, lightDir));
    float specular = pow(saturate(dot(normal, halfDir)), max(specularPower, 1.0f));
    return (ndotl + specular * 0.35f) * lightColor * intensity;
}
