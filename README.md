# DirectX11Demo
Use AMD/MS's codes to pratise dx11 



1.Decal

2.GodRay
	2.1 DeferredVoxelCutoutRender延迟绘制：将在体素内的3D像素标记为黑色；将体素在2D覆盖但却不在3D体素包围空间内标记为漏光颜色vCommonColor4
	2.2 SphereRender使用新的depthStencil绘制一张外延漏光最大范围stencil，只写stencil用于后续blend操作
	2.3 RadialBlurRender将2.1中产生的texture在2.2中stencil范围内进行径向模糊
	2.4 ScreenBlendRender将2.3中生成的texture混合到场景中
