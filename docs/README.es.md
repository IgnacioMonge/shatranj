# Compilar Shatranj

[English](README.md) · [Guía de usuario](../README.es.md) · [Guía del cliente Qt](../client/README.es.md)

El repositorio incluye el código compartido del juego, los cuatro clientes,
recursos, herramientas de empaquetado y pruebas. Los paquetes listos para usar
están en la [última release](https://github.com/IgnacioMonge/shatranj/releases/latest).

## Requisitos

- GNU Make, una shell POSIX y un compilador C99 para las pruebas de host.
- Python 3.10 o posterior con Pillow para generar recursos y probar herramientas.
- z88dk `v24860-762578732` para Spectrum. CI fija esa versión de la herramienta.
- Qt 6, CMake y un compilador C++ compatible para escritorio; consulta las
  [instrucciones por plataforma](../client/README.es.md).
- El SDK del driver SpectraNext para el cartucho, indicado mediante `SPXN_DIR`.

## Compilación y pruebas

Ejecuta los targets desde la raíz del repositorio:

```sh
make test          # Pruebas de host y vectores Z80
make client-test   # Compilar y probar el cliente Qt
make tap           # Classic: release/SHATRANJ.tap, .OVL y .DAT
make nex           # Next: NEX autocontenido
make client        # Empaquetar el cliente de escritorio
make full-check    # Host, límites de módulos, ABI y memoria
```

`full-check` ya recompila Classic y Next. Para paquetes de release se usa
`make release-check`: las mismas comprobaciones con el presupuesto de
optimización del compilador de release. Conserva juntos el TAP, OVL y DAT
de Classic de una misma compilación.

Instalador y paquetes HTTP(S) de Spectranext:

```sh
make spectranext-port-build spectranext-direct SPXN_DIR=/path/to/SpectraNext/driver
make spectranext-pages SPXN_DIR=/path/to/SpectraNext/driver
```

La [guía de arranque HTTP(S)](spectranext-direct.md) explica la distribución de
recursos y los requisitos de firmware. Puedes pasar `PORT`, `MQTT_HOST`,
`MQTT_PORT` y `MQTT_CODE` a Make para generar paquetes Spectrum preconfigurados.

## Mapa del código

| Directorio | Contenido |
| --- | --- |
| `src/common/` | Ajedrez, protocolo, sesión y formato de partidas compartidos |
| `src/spectrum/`, `asm/` | Runtime Spectrum, overlays y ensamblador por plataforma |
| `src/pc/`, `client/` | Aplicación Qt y entradas de compilación de escritorio |
| `assets/` | Gráficos, fuentes e iconos del juego |
| `tools/`, `packaging/` | Generación de recursos, validación y empaquetado |
| `tests/` | Pruebas de regresión y fixtures |

## Referencia

- [Protocolo de red](wire-contract.md): mensajes Direct TCP y MQTT, y compatibilidad.
- [Comprobaciones ABI](abi-guard.md): contratos de overlays Classic y Next.
- [Comprobaciones de tamaño](size-report.md): límites de memoria y referencias.
- [1.4 Open Match](releases/1.4.md): notas de la release.
- [Changelog](../CHANGELOG.md): cambios publicados y pendientes.
