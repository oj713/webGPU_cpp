# Guide de WebGPU - C++ (avec compatabilité Emscripten): Exemples

Cette base de code suit le [Learn WebGPU for C++ Guide](https://eliemichel.github.io/LearnWebGPU/index.html) détaillé par Elie Michel. 

L'objectif est d'acquérir des compétences en C++, CMake, Emscripten, et surtout WebGPU — un moteur graphique de nouvelle génération pour le web. 

Les branches correspondent à des étapes différents du project — ajouter une fenêtre, rendre une texture complexe, etc. Le numéro d'étape aligne avec l'une de [LearnWebGPU-Code](https://github.com/eliemichel/LearnWebGPU-Code) et il y a aussi un court texte déscriptif. 

`notes.md` contient des notes générales pour accompagner le code. 

## Execution

Créez les dossiers de construction pour le project. Isolez les dossiers crées en les plaçant dans une directorie *build/* avec l'option `-B build`. 
* Notez que WebGPU n'est qu'un dossier header donc il existe plusieurs options pour l'implementation. Utilisez les drapeaus pour choisir votre backend préféré. 

Construisez le logiciel et générer l'exécutable. 

Exécutez le programme. 

```
cmake . -B build
# OU
cmake -B build-wgpu -DWEBGPU_BACKEND=WGPU
# OU
cmake -B build-dawn -DWEBGPU_BACKEND=DAWN
cmake --build build
build/App
```

## Dépendances

Ce project utilise [glfw](https://www.glfw.org/) pour fournir une interface commun à travers les OS pour la gestion des fenêtres; [glfw3webgpu](https://github.com/eliemichel/glfw3webgpu) pour relier glfw et webGPU; et [webGPU-C++](https://github.com/eliemichel/WebGPU-Cpp) pour améliorer l'interface C standard de WebGPU.