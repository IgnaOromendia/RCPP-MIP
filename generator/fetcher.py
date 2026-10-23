import sys
import time

import requests # type: ignore
from importer import Importer

class Fetcher:
    DEFAULT_ENDPOINTS = (
        "https://overpass-api.de/api/interpreter",
        "https://overpass.private.coffee/api/interpreter",
    )
    RETRYABLE_STATUS_CODES = {408, 429, 500, 502, 503, 504}
    SUPPORTED_HIGHWAYS = (
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "unclassified", "residential", "motorway_link", "trunk_link",
        "primary_link", "secondary_link", "tertiary_link",
    )
    USER_AGENT = (
        "RCPP-MIP-osm-generator/1.0 "
        "(+https://github.com/IgnaOromendia/RCPP-MIP)"
    )

    def __init__(self, importer=None, http_client=requests, timeout=90,
                 endpoints=None, retries=2, retry_delay=2, sleep=time.sleep):
        self.endpoints = tuple(endpoints or self.DEFAULT_ENDPOINTS)
        if not self.endpoints:
            raise ValueError("se requiere al menos un endpoint de Overpass")
        if retries < 1:
            raise ValueError("los reintentos de Overpass deben ser positivos")
        if retry_delay < 0:
            raise ValueError("la espera entre reintentos no puede ser negativa")
        # Conservado para los consumidores que consultaban el endpoint primario.
        self.url = self.endpoints[0]
        self.importer = importer or Importer()
        self.http_client = http_client
        self.timeout = timeout
        self.retries = retries
        self.retry_delay = retry_delay
        self.sleep = sleep
        

    def fetchZoneFrom(self, geoJSONFile):
        geoJSON     = self.importer.importGeoJSON(geoJSONFile)
        coordinates = self._coordinatesFrom(geoJSON)
        parameters = self._APIParametersFrom(coordinates)
        errors = []
        attempt = 0
        maximum_attempts = self.retries * len(self.endpoints)
        for _ in range(self.retries):
            for endpoint in self.endpoints:
                attempt += 1
                try:
                    # Overpass rechaza o limita clientes con el User-Agent
                    # genérico de requests. POST evita además una URL larga.
                    response = self.http_client.post(
                        endpoint,
                        data=parameters,
                        headers={"User-Agent": self.USER_AGENT},
                        timeout=self.timeout,
                    )
                    response.raise_for_status()
                    return response.json()
                except (requests.RequestException, ValueError) as error:
                    errors.append(f"{endpoint}: {error}")
                    if not self._is_retryable(error):
                        maximum_attempts = attempt
                        break
                    if attempt < maximum_attempts:
                        print(
                            f"Overpass fallo en {endpoint} ({error}); "
                            "probando otro endpoint...",
                            file=sys.stderr,
                        )
                        self.sleep(self.retry_delay * attempt)
            else:
                continue
            break
        detail = "; ".join(errors)
        raise RuntimeError(
            f"No se pudo descargar '{geoJSONFile}' desde Overpass despues de "
            f"{maximum_attempts} intento(s): {detail}"
        )

    @classmethod
    def _is_retryable(cls, error):
        if isinstance(error, requests.HTTPError):
            response = getattr(error, "response", None)
            return (response is not None
                    and response.status_code in cls.RETRYABLE_STATUS_CODES)
        return isinstance(error, (requests.ConnectionError, requests.Timeout,
                                  ValueError))
            
    # Private
    
    def _coordinatesFrom(self,geoJSON):
        return " ".join([f"{lat} {lon}" for lon, lat in  geoJSON['features'][0]['geometry']['coordinates'][0]])
    
    def _APIParametersFrom(self,coords_string):
        highway_pattern = "|".join(self.SUPPORTED_HIGHWAYS)
        query = (
            '[out:json][timeout:25];'
            f'way["highway"~"^({highway_pattern})$"](poly:"{coords_string}");'
            'out body;>;out skel qt;'
        )
        return {'data': query}
