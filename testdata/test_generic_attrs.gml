<?xml version="1.0" encoding="UTF-8"?>
<CityModel xmlns="http://www.opengis.net/citygml/2.0"
           xmlns:bldg="http://www.opengis.net/citygml/building/2.0"
           xmlns:gen="http://www.opengis.net/citygml/generics/2.0"
           xmlns:gml="http://www.opengis.net/gml">
  <gml:name>Test Generic Attributes</gml:name>
  <gml:boundedBy>
    <gml:Envelope srsDimension="3" srsName="EPSG:4326">
      <gml:lowerCorner>0 0 0</gml:lowerCorner>
      <gml:upperCorner>100 100 50</gml:upperCorner>
    </gml:Envelope>
  </gml:boundedBy>

  <cityObjectMember>
    <bldg:Building gml:id="Building_TestGenAttr">
      <!-- 原有属性 -->
      <gml:name>Test Building</gml:name>
      <gml:description>A test building for generic attributes</gml:description>
      <bldg:class>BuildingClass1</bldg:class>
      <bldg:function>Residential</bldg:function>
      <bldg:usage>SingleFamily</bldg:usage>
      <bldg:roofType>gable</bldg:roofType>
      <bldg:measuredHeight uom="m">12.5</bldg:measuredHeight>
      <bldg:storeysAboveGround>3</bldg:storeysAboveGround>
      <bldg:storeysBelowGround>1</bldg:storeysBelowGround>

      <!-- gen:stringAttribute -->
      <gen:stringAttribute name="owner">
        <gen:value>张三</gen:value>
      </gen:stringAttribute>
      <gen:stringAttribute name="material">Brick</gen:stringAttribute>

      <!-- gen:measureAttribute (带单位) -->
      <gen:measureAttribute name="area">
        <gen:value uom="m2">250.5</gen:value>
      </gen:measureAttribute>
      <gen:measureAttribute name="volume">
        <gen:value uom="m3">3000.75</gen:value>
      </gen:measureAttribute>

      <!-- gen:intAttribute -->
      <gen:intAttribute name="yearBuilt">
        <gen:value>1995</gen:value>
      </gen:intAttribute>
      <gen:intAttribute name="rooms">5</gen:intAttribute>

      <!-- gen:doubleAttribute -->
      <gen:doubleAttribute name="efficiency">
        <gen:value>0.85</gen:value>
      </gen:doubleAttribute>
      <gen:doubleAttribute name="elevation">156.7</gen:doubleAttribute>

      <!-- gen:booleanAttribute -->
      <gen:booleanAttribute name="heritage">true</gen:booleanAttribute>
      <gen:booleanAttribute name="inactive">0</gen:booleanAttribute>

      <!-- gen:dateAttribute -->
      <gen:dateAttribute name="inspectionDate">
        <gen:value>2023-06-15</gen:value>
      </gen:dateAttribute>
      <gen:dateAttribute name="completionDate">2020-01-01</gen:dateAttribute>

      <!-- gen:uriAttribute -->
      <gen:uriAttribute name="reference">
        <gen:value>http://example.com/building/123</gen:value>
      </gen:uriAttribute>
      <gen:uriAttribute name="document">https://docs.example.org/bldg.pdf</gen:uriAttribute>

      <!-- gen:linkAttribute -->
      <gen:linkAttribute name="ownerLink">
        <gen:value xlink:href="http://example.com/person/456"/>
      </gen:linkAttribute>
      <gen:linkAttribute name="photoLink">
        <gen:value href="http://example.com/photos/bldg001.jpg"/>
      </gen:linkAttribute>

      <!-- 不带前缀版本测试 -->
      <stringAttribute name="descriptionString">
        <value>不带命名空间前缀的字符串属性</value>
      </stringAttribute>
      <measureAttribute name="height">
        <value uom="m">15.25</value>
      </measureAttribute>
      <intAttribute name="capacity">
        <value>100</value>
      </intAttribute>
    </bldg:Building>
  </cityObjectMember>
</CityModel>
