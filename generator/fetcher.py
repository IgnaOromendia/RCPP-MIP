import requests # type: ignore
from importer import Importer

class Fetcher:
    SUPPORTED_HIGHWAYS = (
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "unclassified", "residential", "motorway_link", "trunk_link",
        "primary_link", "secondary_link", "tertiary_link",
    )
    USER_AGENT = (
        "RCPP-MIP-osm-generator/1.0 "
        "(+https://github.com/IgnaOromendia/RCPP-MIP)"
    )

    def __init__(self, importer=None, http_client=requests, timeout=30):
        self.url = 'https://overpass-api.de/api/interpreter'
        self.importer = importer or Importer()
        self.http_client = http_client
        self.timeout = timeout
        

    def fetchZoneFrom(self, geoJSONFile):
        geoJSON     = self.importer.importGeoJSON(geoJSONFile)
        coordinates = self._coordinatesFrom(geoJSON)
        try:
            # Overpass rechaza o limita clientes con el User-Agent genérico de
            # requests. POST evita además una URL muy larga para el polígono.
            response = self.http_client.post(
                self.url,
                data=self._APIParametersFrom(coordinates),
                headers={"User-Agent": self.USER_AGENT},
                timeout=self.timeout,
            )
            response.raise_for_status()
            return response.json()
        except (requests.RequestException, ValueError) as error:
            raise RuntimeError(
                f"No se pudo descargar '{geoJSONFile}' desde Overpass: {error}"
            ) from error
            
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
