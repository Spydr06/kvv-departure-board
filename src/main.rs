#![feature(let_chains)]

use std::fmt;
use colored::Colorize;

#[derive(Debug, serde::Deserialize)]
struct KVVServingLine {
    number: String,
    direction: String,
    delay: Option<String>
}

impl fmt::Display for KVVServingLine {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let number = match self.number.as_str() {
            "S4" => "S4".white().on_truecolor(102, 25, 36),
            "RE 45" => "RE 45".white().on_truecolor(100, 100, 100),
            x => x.white()
        };
        write!(f, "{}\tto {}", number, self.direction) 
    }
}

#[derive(Debug, serde::Deserialize)]
struct KVVDeparture {
    stopName: String,
    countdown: String,
    platform: String,
    servingLine: KVVServingLine,

}

impl fmt::Display for KVVDeparture {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "In {} ", self.countdown)?;
        if let Some(delay) = &self.servingLine.delay && delay.ne("0") {
            write!(f, "{}", format!("(+{}) ", delay).yellow())?;
        }
        write!(f, "min:\t(Gleis {})\t{}", self.platform, self.servingLine)
    }
}

#[derive(Debug, serde::Deserialize)]
struct KVVResponse {
    departureList: Vec<KVVDeparture>
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let station_id = 7001530; 
    let request_url = format!("https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&locationServerActive=1&mode=direct&name_dm={station_id}&type_dm=stop&useOnlyStops=1&useRealtime=1&limit=10");

    let response = reqwest::get(&request_url).await?.json::<KVVResponse>().await?;
    println!("{:#?}", response);

    println!("Departures from {}:", response.departureList.first().unwrap().stopName);
    for dep in response.departureList {
        println!("{}", dep);
    }

    Ok(())
}
