float3 EvaluateDirectionalLight(float3 normal, float3 viewDir, float3 lightDirection, float3 lightColor, float intensity, float diffuseIntensity, float specularPower, float specularIntensity)
{
    float3 lightDir = normalize(-lightDirection);
    float3 halfDir = normalize(lightDir + viewDir);
    float ndotl = saturate(dot(normal, lightDir));
    float specular = pow(saturate(dot(normal, halfDir)), max(specularPower, 1.0f));
    return lightColor * intensity * (ndotl * diffuseIntensity + specular * specularIntensity);
}
