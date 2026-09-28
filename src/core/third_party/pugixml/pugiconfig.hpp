// pugiconfig.hpp — Configuración de pugixml para FUSION-HP (incluido
// automáticamente por pugixml.hpp en TODAS las TUs: la configuración es
// coherente por construcción).
//
// - Sin excepciones: load_buffer informa el error por xml_parse_result
//   (política del producto: errores explícitos, sin excepciones cruzando
//   la frontera del núcleo).
// - Sin STL ni XPath: no se usan; binario más pequeño.

#pragma once

#define PUGIXML_NO_EXCEPTIONS
#define PUGIXML_NO_STL
#define PUGIXML_NO_XPATH
