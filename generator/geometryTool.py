from shapely.geometry import Point, Polygon # type: ignore
import numpy as np

class GeometryTool:
    def __init__(self):
        pass

    def earthDistance(srcLat, srcLon, dstLat, dstLon):
        # Radio de la Tierra en metros
        R = 6371000  
        
        # Convertir grados a radianes
        phi1 = np.radians(srcLat)
        phi2 = np.radians(dstLat)
        #delta_phi = np.radians(dstLat - srcLat)
        #delta_lambda = np.radians(dstLon - srcLon)
        delta_phi = phi2-phi1
        delta_lambda = np.radians(dstLon) - np.radians(srcLon)
        
        # Fórmula de haversine
        a = np.sin(delta_phi / 2) ** 2 + np.cos(phi1) * np.cos(phi2) * np.sin(delta_lambda / 2) ** 2
        c = 2 * np.arctan2(np.sqrt(a), np.sqrt(1 - a))
        
        # Distancia en metros
        return R * c
    
    def isAPointOf(pointToCheck, points, eps):
        distances = GeometryTool.euclideanDistanceOneToMany(pointToCheck, points)
        return min(distances) < eps

    def euclideanDistanceOneToMany(v, A):
        # Devuelve un vector de distancias euclideas donde Di = distancia euclidea entre fila i de A y v
        X = A - v
        return np.linalg.norm(X, axis=1)
    
    def polygonContainsLine(srcPoint, dstPoint, polygon):
        point1 = Point(srcPoint.y, srcPoint.x)
        point2 = Point(dstPoint.y, dstPoint.x)
        return polygon.contains(point1) and polygon.contains(point2)
    
    def centroidOf(point1, point2):
        lat1, lon1 = point1 # Abajo a la izq
        lat2, lon2 = point2 # Arriba a la der

        C_lat = (lat1 + lat2) / 2
        C_lon = (lon1 + lon2) / 2

        return C_lat, C_lon
    
    def orientedAngle(u, v, w):
        cur_street = v - u
        turn_street = w - v

        # Angulo orientado entre dos vectores -> https://en.wikipedia.org/wiki/Atan2
        dot_prod = np.inner(cur_street, turn_street)   # Dot product between [x1, y1] and [x2, y2]
        # np.cross dejó de admitir vectores 2D en versiones recientes de NumPy.
        # El determinante escalar 2D conserva el signo y la magnitud necesarios
        # para atan2 sin promover artificialmente las coordenadas a 3D.
        det = (cur_street[0] * turn_street[1]
               - cur_street[1] * turn_street[0])
        
        return np.arctan2(det, dot_prod)

    def givensMatrix(theta):
        cos = np.cos(theta)
        sin = np.sin(theta)
        G = np.array([[cos, -sin], [sin, cos]])

        assert(np.allclose(G.T @ G, np.eye(2)))
        assert(np.allclose(np.linalg.norm(G[:,0]),1))
        assert(np.allclose(np.linalg.norm(G[:,1]),1))
        
        return G

    def rotatePolygon(corners, theta):
        cartesian_corners = GeometryTool.geographicToCartesian(corners)

        center = cartesian_corners.mean(axis=0)

        # Centramos 
        corners_centered = cartesian_corners - center

        # Rotamos con la matriz de Givens a cada punto centrado del poligono
        G = GeometryTool.givensMatrix(theta)
        corners_rot = cartesian_corners
        corners_rot[0] = G @ corners_centered[0]
        corners_rot[1] = G @ corners_centered[1]
        corners_rot[2] = G @ corners_centered[2]
        corners_rot[3] = G @ corners_centered[3]

        # Volvemos a trasladar a su lugar original
        corners_rot += center

        return GeometryTool.cartesianToGeographic(corners_rot)
    
    def geographicToCartesian(geographic_corners):
        R = 6371  # Earth radius in km
        cartesian_corners = []

        for lat, lon in geographic_corners:
            lat_rad = np.radians(lat)
            lon_rad = np.radians(lon)
            
            x = R * lon_rad
            y = R * np.log(np.tan(np.pi / 4 + lat_rad / 2))

            cartesian_corners.append((x, y))
        
        return np.array(cartesian_corners)

    def cartesianToGeographic(cartesian_corners):
        R = 6371  # Earth radius in km
        geographic_corners = []
        
        for x, y in cartesian_corners:
            lon = np.degrees(x / R)
            #lat = np.degrees(2 * np.atan(np.exp(y / R)) - np.pi / 2)
            lat = np.degrees(np.arctan(np.sinh(y / R)))
            
            geographic_corners.append((lat, lon))

        return np.array(geographic_corners)

    def newPointAtDistance(A, B, alfa):
        # Caluclamos el vector AB normalizado
        vAB = np.array([B[0] - A[0], B[1] - A[1]]) 
        vAB_norm = vAB / np.linalg.norm(vAB)

        # Caluclamos el vector AC (misma dirección que AB) pero a distancia alfa
        x = A[0] + alfa * vAB_norm[0]
        y = A[1] + alfa * vAB_norm[1]

        return x, y
    
    def newPointFromVectorSum(A, B, C):
        # Obtenemos los vectores AB y AC
        vAB = (B[0] - A[0], B[1] - A[1])
        vAC = (C[0] - A[0], C[1] - A[1])

        # Vector AD resultado de la suma AB + AC
        vAD = (vAB[0] + vAC[0], vAB[1] + vAC[1])

        # Coordenadas del punto D siendo A + AD
        xD = A[0] + vAD[0]
        yD = A[1] + vAD[1]

        return xD, yD
