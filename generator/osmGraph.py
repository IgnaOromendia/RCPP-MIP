from shapely.geometry import shape, Point, Polygon # type: ignore
import xml.etree.ElementTree as ET
import numpy as np
from geometryTool import GeometryTool as gt
from importer import Importer

class OsmGraph:
    def __init__(self, graph, geoJSONData, depositCoords, paths=None):
        self.graph = graph

        self.osm = ET.Element("osm", version="0.6", generator="Overpass API")

        self.polyZone = shape(geoJSONData["features"][0]["geometry"])

        self.depositCoords = depositCoords

        self.speedCobblestoneReduction = 0.85

        #Points
        self.points = {}
        self.srcCobblestonePoints = []
        self.dstCobblestonePoints = []

        # Nodes
        self.nodes                  = []
        self.trainNodes             = []
        self.srcCobblestoneNodes    = []
        self.dstCobblestoneNodes    = []
        self.nearstNodesToDepo      = []
        self.semaphoreNodes         = []
        self.leftTurnSemaphoreNodes = []

        # Ways
        self.wayName = {}
        self.cobblestoneStreets = []
        
        # Coordinates
        self.nodesCoordinates = {}

        # Map from OSM nodes to 1...n
        self.nodeOsmToNodeMap = {}
        self.nodeToNodeOsmMap = {}

        # Curves
        self.curves = []
        self.illegal_curves = []

        # Curve threshold
        self.THRESHOLD = np.pi / 4

        # Imports
        importer = Importer(paths=paths)
        
        self.srcCobblestonePoints, self.dstCobblestonePoints = importer.importcobblestoneStreets()
        self.trainCrossesPoints = importer.importTrainCrosses()
        self.restrictedAreas    = importer.importRestrictedAreas("Barrios-Informales.kml")
        self.restrictedAreas   += importer.importRestrictedAreas("Barrios-cerrados.kml")
        self.semaphorePoints    = importer.importSemaphores()
        self.leftTurnSemaphorePoints = importer.importLeftTurnSemaphores()

    def addElementsFrom(self, zoneJSON, mapName):
        nodeElementMap = self.mapNodes(zoneJSON)
        edgeID = 1
        nodeID = 1

        #PAU
        #Esta lista tiene los no obligatorios sin importar la zona. La direccion es la del osm
        noObligatorios = [(4326330081,12118930451),(4326330081,12118930451),(12118930451,4678722334),(302888374,12118930450),(12118930450,4678722340),(12529131749,333455864),
        (346452870,12118930449),(12118930449,4678722336),(302891301,12118930448),(12118930448,4678722337),(12280151843,302882010),(316628027,4960159736),(5303992591,2935921406),
        (2935921406,2377625561),(4960159736,333455373),(3030982037,7303930335),(7303930335,12522901355),(4590117293,12118900361),(12118900361,333454854),(4590117295,12118900362),
        (12118900362,333472018),(12522547464,302890447),(4949936491,6030702839),(6030702839,6030702838),(6030702838,4949936493),
        (4949936493,4949936490),(2377625561,12312923358),(12312923358,2377625552),(2377625552,5315592076),(5315592076,2377625544),
        (2377625544,2377625541),(2377625541,5303992585),(5303992585,4678722332),(2809271002,421872392),(3030929394,3030929393),(3030929393,3030929392),
        (4886981429,6193915355),(6193915355,8889182027),(8889182027,4886981430),(4886981430,6193915354),(6193915354,6193915351),(6193915351,6193915321),(6193915321,5761065784),(5761065784,334936791),
        (309809283,9301160300),(9301160300,9301160301),(9301160301,247765136),(251758520,2809271002),(2809271002,421872392),(595709683,30419053),(30419053,595709730),(2164748764,2809270695),
        (2809270695,595709727),(595709727,421872342),(685079560,3456812736),(3456812736,2865132122),(2865132122,4329521674),(4329521674,421872351),(243092436,11055572405),
        (11055572405,11055571104),(11055571104,36506709),(6030397725,6030397726),(424140127,6030397726),(6030397725,424140127),(5046992821,2189091101),(2189091101,1719986434),
        (1719986434,2561133550),(2561133550,685055386),(424140175,2189091127),(2189091127,424140178),(424140178,3177996785),(3177996785,424140180),(29650925,2189091093),(2189091093,108267195),
        (108267195,2169672605),(2169672605,5123249978),(5123249978,29650920),(2674983223,12118978812),(12118978812,8842262842),(2674983242,8842262848),(8842262848,4327399258),(4327399287,4327399281),
        (4327400008,685054590),(424171036,4327400020),(9210904464,9210904463),(9210904463,9210904462),(9210904462,9210904461),(9210904461,9210904465),(9210904465,9210904464),
        (9210904464,11523762363),(11523762363,29650925),(5123249978,12525735607),(2809271002,421872392),(30419053,2809270697),(2809270697,595709730),(12525895123,247765134),(12525895122,595715774),
        (12528304348,247765133),(595709605,12525895124),(421854167,12526958972),(12526958975,421854165),(421854164,12526958976),
        (12331400906,12331400904),(12331400904,12331400905),(6030074639,661156041),(661156041,6030074645),(6030074645,661156044),(661156048,6030074642),(6030074642,598517490),(598517490,9935680455),
        (9935680455,6030074648),(661161367,267584160),(267584160,661156051),(4638795404,4638795422),(4638795422,267584123),(267584123,4638795402),(4638795402,4638795409),(4638795409,4638795410),
        (4638795410,4638795403),(4638795403,4638795406),(4638795406,4638795416),(4638795416,4638795405),(4638795405,4638795404),(267584123,4638795423),(4638795423,4638795424),(4638795424,4638795407),
        (4638795407,4638795408),(4638795408,4638795409),(4638795410,4638795411),(4638795411,4638795412),(4638795412,4638795413),(4638795413,4638795414),(4638795414,4638795415),(4638795415,4638795406),
        (4638795405,4638795417),(4638795417,4638795418),(4638795418,4638795419),(4638795419,4638795420),(4638795420,4638795421),(4638795421,4638795422),(4631335668,4631335670),(6016355912,4631335671),
        (4631335671,12522401859),(12522401859,12522401860),(12522401860,598517516),(597717555,4631335669),(597719567,11999458210),(11999458210,11999458209),(11999458209,4627957681),(4627957719,4627957721),
        (4627957721,4627957722),(4627957722,4627957723),(3922847390,6728639640),(1855776710,3922847393),(3922847396,1855776711),(303077976,3922847399),(3922847400,2986113890),(302869454,3922847401),
        (3922847402,6728639639),(3922847403,302893377),(302893561,3922847406),(29651162,3922847407),(3922847409,6728639638),(243092278,9789441677),(6728639636,243092089),(1432951811,1733427709),
        (4327399232,423984183),(4327399227,424171093),(243093280,8075785046),(8075785046,8075785029),(8075785028,8075785043),(8075785043,5767230419),(5767230419,8075785044),(8075785044,8075785042),
        (8075785039,8075785024),(8075785024,8075785049),(8075785049,8075785023),(8075785023,8075785035),(8075785034,8075785045),(8075785045,243093280),(8075785034,8075785033),(8075785033,8075785032),
        (8075785032,8075785031),(8075785031,8075785030),(8075785030,8075785029),(8075785029,9177025507),(9177025507,8075785028),(8075785028,8075785027),(8075785027,8075785026),(8075785026,8075785025),
        (8075785025,8075785042),(8075785042,8075785041),(8075785041,8075785040),(8075785040,8075785039),(8075785039,8075785038),(8075785038,8075785037),(8075785037,8075785036),(8075785036,8075785035),
        (8075785035,8075785034),(3922847390,6728639640),(267584167,8547052128),(8547052128,32729811),(3822115575,12315705738),(12315705738,4340236336),
        (423984181,4327400028),(4327400028,5018197843),(5018197843,424171036),(424171036,12522848895),(12522848895,685054590),(685054590,4327399992),
        (4327399992,4327399281),(4327399281,4327399274),(4327399274,2674983242),
        (424092501,597719565),(597709203,2377625482),(597709203,2377625481),(2377625481,3031323951),(3031323951,12529159448),(12529159448,597717660)]


        #Estas listas tienen las doble manos no obligatorias solo para un lado para esa zona en particular
        martinezA = [(341117230,6050491813),(6050491813,4349315303),(4349315303,898413707),(898413707,12319470004),(12319470004,12319455999),(12319455999,12319470005),(12319470005,346448434),
        (346448434,1372372241),(1372372241,898413827),(898413827,909830208),(909830208,6050491815),(6050491815,595701912),(595701912,330147670),
        (330147670,6050491810),(6050491810,595701914),(595701914,1372457529),(1372457529,6050491817),(6050491817,595701908),(595701908,338511440),
        (338511440,6050491818),(6050491818,595701910),(595701910,339196395),(339196395,6050491820),(6050491820,595701919),(595701919,6050491821),
        (6050491821,339197884),(339197884,595701921),(595701921,6050491808),(6050491808,295059536),(295059536,12520178456),(12520178456,10013082170),
        (10013082170,299742831),(299742831,339197445),(339197445,12518749037),(12518749037,299743392),(299743392,12518749038),(12518749038,339199031),
        (339199031,7092435773),(7092435773,12518749039),(12518749039,10014411992),(10014411992,299742836),(299742836,299742833),(299742833,7092435770),(7092435770,12520425304),
        (12520425304,29647104),(29647104,12520425314),(12520425314,7092435772),(7092435772,12352305103),(12352305103,3404152027),(3404152027,12518749040),
        (12518749040,12520588372),(12520588372,7092435768),(7092435768,12518749041),(12518749041,338407650),(338407650,12352305106),(12352305106,12520588368),
        (12520588368,12518749043),(12518749043,299362052),(299362052,12518749042),(12518749042,7092435766),(7092435766,12520425323),(12520425323,299348114),
        (299348114,12520425327),(12520425327,5762663654),(5762663654,12518212136),(12518212136,6174312798),(6174312798,299348124),(299348124,12520896861),
        (12520896861,9503913407),(9503913407,299363172),(299363172,10208160939),(10208160939,12520896862),(12520896862,299363218),(346455372,11033506464),(11033506464,12518215733),
        (12518215733,12518309730),(12518309730,346452175),(346452175,2293987083),(2293987083,12520283332),(12520283332,12518215746),(12518215746,346452317),
        (346452317,3031164168),(3031164168,12518215750),(12518215750,346452469),(346452469,1855741260),(1855741260,12518215751),(12518215751,12518215752),
        (12518215752,346453899),(346453899,12518215756),(12518215756,346454112),(346454112,346454161),(346454161,12518215754),(12518215754,12518043967),(12518043967,346454427)]

        martinezB = [(295059536,6050491822),(6050491822,595702070),(595702070,595702069),(595702069,6050491806),(6050491806,12523327693),(12523327693,299742832),
        (299742832,12523327694),(12523327694,6050491823),(6050491823,12518225427),(12518225427,299743389),(299743389,6050491826),(6050491826,12541884552),
        (12541884552,299742883),(299742883,12541884551),(12541884551,6050491824),(6050491824,299743402),(299743402,595702072),(595702072,322026438),
        (322026438,12541884550),(12541884550,6051901094),(6051901094,322031261),(322031261,11126565268),(11126565268,595702073),(595702073,12541884549),
        (12541884549,299742909),(299742909,12541884548),(12541884548,6050491827),(6050491827,12541884546),(12541884546,595702067),(595702067,322030439),
        (322030439,12541884547),(12541884547,6050491828),(6050491828,595702074),(595702074,12541884545),(12541884545,322030557),(322030557,6050491832),
        (6050491832,12541884543),(12541884543,299743377),(299743377,12541884544),(12541884544,6050491830),(6050491830,299742869),(299742869,1615000957),
        (1615000957,12541884542),(12541884542,299742857),(299742857,6050491837),(6050491837,12541884541),(12541884541,299749345),(299749345,6050491838),
        (6050491838,12541884540),(12541884540,299742822),(299742822,12541884539),(12541884539,299749294),(299749294,12541884538),(12541884538,299742828),
        (299742828,299749289),(299749289,12541884537),(12541884537,6050491839),(6050491839,252096776),(252096776,898413795),(898413795,12541884536),
        (12541884536,6050491841),(6050491841,12541884534),(12541884534,252096821),(252096821,299742766),(299742766,12541884535),(12541884535,6050491842),
        (6050491842,252096660),(252096660,299742767),(299742767,12541884533),(12541884533,900071711),(900071711,6050491844),(6050491844,6050491804),
        (6050491804,12526823166),(12526823166,29375382),(299363531,9503913408),(9503913408,6174312797),(6174312797,595663631),(595663631,12520896863),
        (12520896863,299363218),(299363218,12520896862),(12520896862,10208160939),(10208160939,299363172),(299363172,9503913407),(9503913407,12520896861),
        (12520896861,299348124),(299348124,6174312798),(6174312798,12518212136),(12518212136,5762663654),(5762663654,12520425327),(12520425327,299348114),
        (299348114,12520425323),(12520425323,7092435766),(7092435766,12518749042),(12518749042,299362052),(299362052,12518749043),(12518749043,12520588368),
        (12520588368,12352305106),(12352305106,338407650),(338407650,12518749041),(12518749041,7092435768),(7092435768,12520588372),(12520588372,3404152027),
        (3404152027,12352305103),(12352305103,7092435772),(7092435772,12520425314),(12520425314,29647104),(29647104,12520425304),(12520425304,7092435770),
        (7092435770,299742833),(299742833,299742836),(299742836,10014411992),(10014411992,12518749039),(12518749039,7092435773),(7092435773,339199031),
        (339199031,12518749038),(12518749038,299743392),(299743392,12518749037),(12518749037,339197445),(339197445,299742831),(299742831,10013082170),
        (10013082170,12520178456),(12520178456,295059536)]

        acassusoA = [(4327399285,12520944000),(12520944000,424171020),(424171020,685055386),(5046992821,9210892857),(9210892857,9210892853),(9210892853,5046992417),
        (5046992417,424140150),(424140150,12352341314),(12352341314,12520973205),(12520973205,29603531)]
        #[(421872336,421872337),(421872337,5047067559),(5047067559,12315876241),(12315876241,3177996779),(3177996779,12315876243),(12315876243,2189091103),
        #(2189091103,12528742706),(12528742706,1871042518),(1871042518,247764965),(247764965,12528742707),(12528742707,29650902),

        acassusoB = []

        sanisidroA = [(661131616,7097548311),(7097548311,1855574431),(1855574431,597710957),(597710957,12518247821),(12518247821,7097548310),(7097548310,12518247822),
        (12518247822,1855574501),(1855574501,12518247823),(12518247823,243092267),(243092267,9852226982),(9852226982,243091869),(243091869,12518247826),
        (12518247826,10005260353),(10005260353,12517714798),(12517714798,302893587),(302893587,7097548307),(7097548307,12517646068),(12517646068,29651086),
        (29651086,12517646076),(12517646076,30597032),(30597032,12517883292),(12517883292,2986126014),(2986126014,12518047291),(12518047291,29651075)]

        sanisidroB = [(29651075,12518047291),(12518047291,2986126014),(2986126014,12517883292),(12517883292,30597032),(30597032,12517646076),(12517646076,29651086),
        (29651086,12517646068),(12517646068,7097548307),(7097548307,302893587),(302893587,12517714798),(12517714798,10005260353),(10005260353,12518247826),
        (12518247826,243091869),(243091869,9852226982),(9852226982,243092267),(243092267,12518247823),(12518247823,1855574501),(1855574501,12518247822),
        (12518247822,7097548310),(7097548310,12518247821),(12518247821,597710957),(597710957,1855574431),(1855574431,7097548311),(7097548311,661131616),
        (5744609374,12518188736),(12518188736,9787180517),(9787180517,12526712124),(12526712124,1855574443),(1855574443,12526712123),(12526712123,9787176715),
        (9787176715,12518247818),(12518247818,267592117),(267592117,12518247817),(12518247817,9787176714),(9787176714,12518247816),(12518247816,1855574451),
        (1855574451,12518247815),(12518247815,9787176713),(9787176713,9787176712),(9787176712,12518247814),(12518247814,1855574442),(1855574442,12518247813),
        (12518247813,9787176711),(9787176711,1855574491),(1855574491,12526712127),(12526712127,1855553618),(1855553618,12526712126),(12526712126,9787176710),
        (9787176710,9787176709),(9787176709,12526712120),(12526712120,1855543713),(1855543713,12526712119),(12526712119,1855553623),(1855553623,12526712117),
        (12526712117,12526712116),(12526712116,1855543719),(1855543719,12526712114),(12526712114,9787176700),(9787176700,12517678154),(12517678154,1855574447),
        (1855574447,9787176708),(9787176708,12517678155),(12517678155,12517678156),(12517678156,303388438),(303388438,12517678157),(12517678157,2174238177)]

        #Estas listas tienen las simple mano no obligatorias para esa zona en particular, pero si obligatorios para otra. Tiene la direccion del osm
        acassusoBOne = [(421872367,421872293),(421872293,421872294),(421872367,4625804155),(4625804155,4625804148),(4625804148,421872292),(421872292,421872291),
        (421872291,421872290),(421872290,421872289),(421872289,421872288),(421872288,421872287),(421872287,421872264),(421872264,421872339),
        (792097268,333465492),(333465492,12522180632),(12522180632,4338770173),(421872294,421872303),(421872303,421872312),(421872312,421872321),
        (421872321,421872330),(421872330,12315876230),(12315876230,421872336)]

        sanisidroAOne = [(424092494,12518461680),(12518461680,29651075)]

        sanisidroBOne = [(424092514,12519623178),(12519623178,3814617870),(3814617870,424092494)]

        mapas = {'acassusoA':acassusoA,'acassusoB':acassusoB,'martinezA':martinezA,'martinezB':martinezB,'sanisidroA':sanisidroA,'sanisidroB':sanisidroB}
        lista = mapas.get(mapName, [])

        mapasNoOblig = {'acassusoB':acassusoBOne,'sanisidroA':sanisidroAOne,'sanisidroB':sanisidroBOne}
        noObligatoriosZona = mapasNoOblig.get(mapName, [])

        # Generamos todos las aristas/arcos
        for element in zoneJSON.get('elements', []):
            #print(element)
            if not self.isAStreetEdge(element): continue

            nodes = element.get('nodes', [])

            # Recorremos todos los nodos de una calle para generar las aristas/arcos necesarios
            for i in range(len(nodes) - 1):
                srcNode = nodes[i]
                dstNode = nodes[i+1]

                # Obtenemos el nodo-elemento OSM
                srcOSMNode = nodeElementMap[srcNode]
                dstOSMNode = nodeElementMap[dstNode]

                # Si uno de los nodos está en una zona restringida no lo agregamos
                if self.inRestrictedArea(srcOSMNode) or self.inRestrictedArea(dstOSMNode): continue
                
                # Agregamos los nodos que une la arista
                srcNewId, nodeID = self.addNode(srcOSMNode, srcNode, nodeID)
                dstNewId, nodeID = self.addNode(dstOSMNode, dstNode, nodeID)

                # Nos fijamos si está en la zona obligatoria
                #PAU
                #isRequested = gt.polygonContainsLine(self.points[srcNode], self.points[dstNode], self.polyZone)
                isRequested = (srcNode,dstNode) not in noObligatorios and (srcNode,dstNode) not in noObligatoriosZona and gt.polygonContainsLine(self.points[srcNode], self.points[dstNode], self.polyZone)

                # Creamos la arsita/arco
                newEdge = ET.SubElement(self.osm, 'way', id=str(edgeID), version="1")

                # Obtenemos los tags
                edgeTags = element.get('tags', {})

                # Agregamos los tags
                for key, value in edgeTags.items(): 
                    if key == 'highway': # Para marcar en el osm si es obligatoria
                        if isRequested: ET.SubElement(newEdge, 'tag', k=key, v='tertiary')
                    else:
                        ET.SubElement(newEdge, 'tag', k=key, v=value)

                # Agregamos referencia al OSM
                ET.SubElement(newEdge, 'nd', ref=str(srcNode))
                ET.SubElement(newEdge, 'nd', ref=str(dstNode))

                # Nos fijamos si es una calle empedrada
                if self.isCobblesoneStreet(srcNode, dstNode):
                    self.cobblestoneStreets.append((srcNode, dstNode))
                    #ET.SubElement(newEdge, 'tag', k='highway', v='motorway')

                # Definimos que tipo es
                #PAU
                #if self.isOneway(edgeTags): 
                #    self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                #else:
                #    self.graph.addEdge(v=srcNewId, u=dstNewId, isRequested=isRequested)

                #if self.isOneway(edgeTags):
                #    self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                #else:
                #    if self.isTwoway(edgeTags):
                #        if (srcNode,dstNode) not in AcassusoA:
                #             self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                #        else:
                #             self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=False)
                #        if (dstNode,srcNode) not in AcassusoA:
                #            self.graph.addArc(v=dstNewId, u=srcNewId, isRequested=isRequested)
                #        else:
                #            self.graph.addArc(v=dstNewId, u=srcNewId, isRequested=False)
                #    else:
                #        self.graph.addEdge(v=srcNewId, u=dstNewId, isRequested=isRequested)

                if self.isOneway(edgeTags):
                    self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                else:
                    if (dstNode,srcNode) in lista:
                        self.graph.addArc(v=dstNewId, u=srcNewId, isRequested=False)
                        self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                    else:
                        if (srcNode,dstNode) in lista:
                            self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=False)
                            self.graph.addArc(v=dstNewId, u=srcNewId, isRequested=isRequested)
                        else:
                            if self.isAvenue(edgeTags):
                                self.graph.addArc(v=srcNewId, u=dstNewId, isRequested=isRequested)
                                self.graph.addArc(v=dstNewId, u=srcNewId, isRequested=isRequested)
                            else:
                                self.graph.addEdge(v=srcNewId, u=dstNewId, isRequested=isRequested)

                # Nos guardamos el nombre de la calle
                self.wayName[(srcNode,dstNode)] = str(edgeTags.get('name', element.get('id')))

                edgeID += 1

    def addNode(self, element, oldNodeID, newNodeID):
        # Si ya fue agregado devolvemos el mismo id que nos llego
        if oldNodeID in self.nodes: return self.nodeOsmToNodeMap[oldNodeID], newNodeID

        lat = element['lat']
        lon = element['lon']

        # Creamos el elemento nodo en OSM
        newNode = ET.SubElement(self.osm, 'node', id=str(oldNodeID), lat=str(lat), lon=str(lon), version="1")

        # chequeamos si es una esquina de una empedrado
        self.checkCobblestonePoint(oldNodeID, lat, lon)

        # chequeamos si es un cruce de tren
        if self.isATrainCross(lat, lon):
            self.trainNodes.append(oldNodeID)

        # chequeamos si es un semaforo
        if self.isASemaphoreNode(lat, lon): self.semaphoreNodes.append(oldNodeID)

        # chequeamos si es un semaforo con giro a la izquierda
        if self.isALeftTurnSemaphoreNode(lat, lon): 
            ET.SubElement(newNode, 'tag', k="highlight", v="yes")
            self.leftTurnSemaphoreNodes.append(oldNodeID)

        # Agregamos los tags
        for tag in element.get('tags', {}):
            ET.SubElement(newNode, 'tag', k=tag, v=element['tags'][tag])

        # Guardamos las coordenadas
        self.nodesCoordinates[oldNodeID] = (float(lat),float(lon))
        self.points[int(oldNodeID)]      = Point(float(lat),float(lon))

        self.nodes.append(oldNodeID)

        # Agregamos el nodo al map
        self.nodeOsmToNodeMap[oldNodeID] = newNodeID
        self.nodeToNodeOsmMap[newNodeID] = oldNodeID

        # Agregamos el nodo a la lista de adyacencia
        self.graph.addNode()

        # Devolvemos el nuevo id del nodo y el siguiente id
        return newNodeID, newNodeID + 1

    def inRestrictedArea(self, nodeElement):
        lat = nodeElement['lat']
        lon = nodeElement['lon']

        for poly in self.restrictedAreas:
            if poly.contains(Point(lon, lat)): return True

        return False

    def isAStreetEdge(self, element):
        streetAttributes = ['motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'unclassified', 'residential', 'motorway_link', 'trunk_link', 'primary_link', 'secondary_link', 'tertiary_link']
        return element['type'] == 'way' and element.get('tags', {}).get('highway') in streetAttributes

    def isOneway(self, tags):
        if tags.get('oneway') in ['yes']: return True
        if tags.get('junction') == 'roundabout': return True
        return False

    #PAU
    def isAvenue(self, tags):
        if 'Avenida' in (tags.get('name') or ''): return True
        return False

    def mapNodes(self, responseJSON):
        elements = responseJSON.get('elements', [])
        return {element['id']: element for element in elements if element['type'] == 'node'}
    
    # Auxiliars

    def isCobblesoneStreet(self, srcNode, dstNode):
        return srcNode in (self.srcCobblestoneNodes + self.dstCobblestoneNodes) and dstNode in (self.srcCobblestoneNodes + self.dstCobblestoneNodes)

    def checkCobblestonePoint(self, nodeId, lat, lon, eps=1e-4):
        if gt.isAPointOf(pointToCheck=np.array([lat,lon]), points=self.srcCobblestonePoints, eps=eps):
            self.srcCobblestoneNodes.append(nodeId)
        elif gt.isAPointOf(pointToCheck=np.array([lat,lon]), points=self.dstCobblestonePoints, eps=eps):
            self.dstCobblestoneNodes.append(nodeId)

    def isATrainCross(self, lat, lon, eps=1e-4):
        return gt.isAPointOf(pointToCheck=np.array([lat,lon]), points=self.trainCrossesPoints, eps=eps)
    
    def isASemaphoreNode(self, lat, lon, eps=1e-4):
        return gt.isAPointOf(pointToCheck=np.array([lat,lon]), points=self.semaphorePoints, eps=eps)

    def isALeftTurnSemaphoreNode(self, lat, lon, eps=1e-4):
        return gt.isAPointOf(pointToCheck=np.array([lat,lon]), points=self.leftTurnSemaphorePoints, eps=eps)
    
    
    # Dead Ends

    def removeDeadEnds(self):
        tree = ET.ElementTree(self.osm)
        root = tree.getroot()

        count = 0

        while True:
            uDeadEnds     = [v for v in self.graph.oneNeighbourNodes()  if self.nodeToNodeOsmMap[v] in self.nodes]
            noEntryNodes  = [v for v in self.graph.dInZeroNodes()       if self.nodeToNodeOsmMap[v] in self.nodes]
            deadEnds      = [v for v in self.graph.dOutZeroNodes()      if self.nodeToNodeOsmMap[v] in self.nodes]

            if (not deadEnds and not noEntryNodes and not uDeadEnds) or count >= 100: break

            if count % 10 == 0: print("#" + str(count) + " Removing " + str(len(uDeadEnds) + len(noEntryNodes) + len(deadEnds)) + " nodes")

            self.removeElements(root, uDeadEnds)
            self.removeElements(root, noEntryNodes)
            self.removeElements(root, deadEnds, useTransposed=True)

            count += 1

    # Remove OSM elements

    def removeElements(self, root, nodesToRemove, useTransposed=False):
        for v in nodesToRemove:

            if useTransposed:   neighbours = self.graph.incidentNodes(v)
            else:               neighbours = self.graph.neighbours(v)

            for u in neighbours:
                self.removeWay(root, v, u)

    def removeWay(self, root, v, u):
        for way in root.findall('.//way'):
            nodes = [int(nd.get('ref')) for nd in way.findall('nd')]

            if (self.nodeToNodeOsmMap[v] in nodes and self.nodeToNodeOsmMap[u] in nodes):
                root.remove(way) 
                self.graph.removeEdge(v,u)
                self.removeNode(root, self.nodeToNodeOsmMap[v])
                break  
    
    def removeNode(self, root, nodeToRemove):
        if nodeToRemove in self.nodes: self.nodes.remove(nodeToRemove)

        for node in root.findall('.//node'):
            if int(node.get('id')) == nodeToRemove:
                root.remove(node)
                break
    
    # Reassing

    def reassingAdjListIds(self):
        newIdMap = self.graph.reassingIds()
        self.reassingCoordinates(newIdMap)
        self.reassingMap(newIdMap)
        self.reassingIdsInAttributes()

    def reassingCoordinates(self, newIdMap):
        coordinates = list(self.nodesCoordinates.items())

        self.nodesCoordinates = {}

        for (node, coords) in coordinates:
            if self.nodeOsmToNodeMap[node] in newIdMap:
                self.nodesCoordinates[node] = coords

    def reassingMap(self, newIdMap):
        idMap = list(self.nodeOsmToNodeMap.items())

        self.nodeOsmToNodeMap = {}
        self.nodeToNodeOsmMap = {}

        for (osmNode, node) in idMap:
            if osmNode in self.nodes and node in newIdMap:
                self.nodeOsmToNodeMap[osmNode] = newIdMap[node]
                self.nodeToNodeOsmMap[newIdMap[node]] = osmNode

    def reassingIdsInAttributes(self):
        trainNodesCopy = self.trainNodes.copy()
        cobblestoneStreetCopy = self.cobblestoneStreets.copy()
        pointsCopy = list(self.points.items())
        wayNamesCopy = list(self.wayName.items())

        self.trainNodes.clear()
        self.cobblestoneStreets.clear()
        self.points.clear()
        self.wayName.clear()

        for node in trainNodesCopy:
            if not node in self.nodeOsmToNodeMap.keys(): continue
            self.trainNodes.append(self.nodeOsmToNodeMap[node])

        for src, dst in cobblestoneStreetCopy:
            if not src in self.nodes or not dst in self.nodes: continue
            self.cobblestoneStreets.append((self.nodeOsmToNodeMap[src], self.nodeOsmToNodeMap[dst]))

        for node, point in pointsCopy:
            if not node in self.nodeOsmToNodeMap.keys(): continue
            self.points[self.nodeOsmToNodeMap[node]] = point

        for (v, u), name in wayNamesCopy:
            if not (v in self.nodeOsmToNodeMap.keys() and u in self.nodeOsmToNodeMap.keys()): continue
            way = (self.nodeOsmToNodeMap[v], self.nodeOsmToNodeMap[u])
            self.wayName[way] = name

    # Distances

    def nodeEarthDistance(self, v, u):
        srcLat, srcLon = tuple(map(float,self.nodesCoordinates[self.nodeToNodeOsmMap[v]]))
        dstLat, dstLon = tuple(map(float,self.nodesCoordinates[self.nodeToNodeOsmMap[u]]))
        return gt.earthDistance(srcLat, srcLon, dstLat, dstLon)   

    def calculateGraphMetrics(self, truckSpeed, streetDemand):
        n = self.graph.n

        self.graph.dist = [[0] * n for _ in range(n)]
        self.graph.time = [[0] * n for _ in range(n)]
        self.graph.demand = [[0] * n for _ in range(n)]

        for v in range(1,n):
            for u in self.graph.neighbours(v):
                trainCross  = int(v in self.trainNodes or u in self.trainNodes)
                speed       = truckSpeed

                if (v, u) in self.cobblestoneStreets: speed *= self.speedCobblestoneReduction

                self.graph.dist[v][u]     = self.nodeEarthDistance(v, u)
                self.graph.demand[v][u]   = streetDemand * self.graph.dist[v][u] / 100
                self.graph.time[v][u]     = (self.graph.dist[v][u] / speed) * (1 + 0.5 * trainCross)

    def detectNearstNodesToDeposit(self):
        depositDist = [10e8] * self.graph.n

        for v in range(1, self.graph.n):
            vLat, vLon = tuple(map(float, self.nodesCoordinates[self.nodeToNodeOsmMap[v]]))
            depositDist[v] = gt.earthDistance(self.depositCoords[0], self.depositCoords[1], vLat, vLon)

        depositDist = np.array(depositDist)

        indexSorted = np.argsort(depositDist)

        self.nearstNodesToDepo = indexSorted[0:19:2]

        # self.nearstNodesToDepo[-1] = self.nodeOsmToNodeMap[287984057]

    # Coords

    def nodesCoords(self):
        return [(float(x),float(y)) for x,y in self.nodesCoordinates.values()]

    # Curves

    def detectCurves(self):
        for (u,v), currStreetName in self.wayName.items():
            for w in self.graph.neighbours(v):
                turnStreetName = self.wayName.get((v,w))
                # Los retornos en U no son conectores de giro del supergrafo.
                # Tampoco hay curva si el tramo no avanza o conserva la calle.
                if u == w or w == v or currStreetName == turnStreetName: continue
                self.curves.append((u,v,w))

                # Si la calle no es doble mano, ninguna tripla que la involucre puede ser ilegal
                if not (self.graph.isAnEdge(v,u) or "Avenida" in currStreetName): continue

                # Si la calle tiene seamforo con giro a la izquierda no es ilegal
                if self.nodeToNodeOsmMap[v] in self.leftTurnSemaphoreNodes: continue

                # Si la calle no tiene semaforo se puede girar a la izquierda mientras que no sea una avenida
                if not self.nodeToNodeOsmMap[v] in self.semaphoreNodes and not "Avenida" in currStreetName: continue

                c_u = self.nodesCoordinates[self.nodeToNodeOsmMap[u]]
                c_v = self.nodesCoordinates[self.nodeToNodeOsmMap[v]]
                c_w = self.nodesCoordinates[self.nodeToNodeOsmMap[w]]

                c_u = np.flip(np.array(c_u, dtype=np.double))
                c_v = np.flip(np.array(c_v, dtype=np.double))
                c_w = np.flip(np.array(c_w, dtype=np.double))

                angle = gt.orientedAngle(c_u, c_v, c_w)

                # Medimos si el giro es mas pronunciado que self.THRESHOLD (esta en radianes)
                if (self.THRESHOLD < angle < np.pi):
                    self.illegal_curves.append((u, v, w))

    def angleOfStreet(self, referenceStreet):
        westernPoint = (float('inf'),0)
        easternPoint = (-float('inf'),0)

        for (src, dst), stretName in self.wayName.items():
            if stretName == referenceStreet:
                srcCoords = self.nodesCoordinates[self.nodeToNodeOsmMap[src]]
                dstCoords = self.nodesCoordinates[self.nodeToNodeOsmMap[dst]]

                if srcCoords[0] < westernPoint[0]:
                    westernPoint = srcCoords

                if dstCoords[0] < westernPoint[0]:
                    westernPoint = dstCoords

                if srcCoords[0] > easternPoint[0]:
                    easternPoint = srcCoords

                if dstCoords[0] > easternPoint[0]:
                    easternPoint = dstCoords

        if westernPoint[0] == float('inf') or easternPoint[0] == -float('inf'):
            raise ValueError(
                f"La calle de referencia '{referenceStreet}' no existe en el grafo."
            )

        # Obtenemos el angulo de la calle con respecto a la latitud paralela al ecuador
        hipotenuse = gt.earthDistance(westernPoint[0], westernPoint[1], easternPoint[0], easternPoint[1])
        if hipotenuse == 0:
            raise ValueError(
                f"La calle de referencia '{referenceStreet}' no define una orientación."
            )
        opposite = gt.earthDistance(westernPoint[0], westernPoint[1], westernPoint[0], easternPoint[1])

        theta = np.arccos(np.clip(opposite / hipotenuse, -1.0, 1.0))

        return theta
