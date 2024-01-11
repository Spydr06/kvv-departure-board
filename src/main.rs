#![feature(let_chains)]

use std::{fmt, collections::HashSet, borrow::BorrowMut};
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
            "S5" => "S5".black().on_truecolor(234, 165 ,100),
            "S2" => "S2".white().on_truecolor(113, 66, 183),
            "RE 45" => "RE 45".white().on_truecolor(100, 100, 100),
            x => x.white()
        };
        write!(f, "{}\tto {}", number, self.direction) 
    }
}

#[derive(Debug, serde::Deserialize)]
struct KVVInfoText {
    subtitle: String
}

#[derive(Debug, serde::Deserialize)]
struct KVVLineInfo {
    infoText: KVVInfoText
}

#[derive(Debug, serde::Deserialize)]
struct KVVLineInfos {
    lineInfo: KVVLineInfo
}

#[derive(Debug, serde::Deserialize)]
struct KVVDeparture {
//    stopName: String,
//    countdown: String,
//    platform: String,
//    servingLine: KVVServingLine,
//    lineInfos: Option<KVVLineInfos>
}

impl fmt::Display for KVVDeparture {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
 //       write!(f, "In {} ", self.countdown) // ?;
/*       if let Some(delay) = &self.servingLine.delay {
            if delay.eq("-9999") {
                write!(f, "{}", "** ".bright_red())?;
            }
            else if delay.ne("0") {
                write!(f, "{}", format!("(+{}) ", delay).yellow())?;
            }
        }
        write!(f, "min:\t(Gleis {})\t{}", self.platform, self.servingLine) */
        Ok(())
    }
}

#[derive(Debug, serde::Deserialize)]
struct KVVResponse {
    departureList: Option<Vec<KVVDeparture>>
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    //let station_id = 7001530; 
    let station_id = 7000801;
    let limit = 1;
    let request_url = format!("https://projekte.kvv-efa.de/sl3-alone/XSLT_DM_REQUEST?outputFormat=JSON&coordOutputFormat=WGS84[dd.ddddd]&depType=stopEvents&locationServerActive=1&mode=direct&name_dm={station_id}&type_dm=stop&useOnlyStops=1&useRealtime=1&limit={limit}");

    println!("<<< {:?}", request_url);

    let request_response = reqwest::get(&request_url).await?;

    let response = request_response.json::<KVVResponse>().await?;
    println!(">>> {:#?}", response);

/*    println!("Departures from {}:", response.departureList.first().unwrap().stopName);
    for dep in &response.departureList {
        println!("{}", dep);
    }

    let line_infos = HashSet::<&String>::from_iter(
        response.departureList.iter()
            .filter_map(|i| i.lineInfos.as_ref())
            .map(|i| &i.lineInfo.infoText.subtitle)
        );
    if line_infos.len() > 0 {
        println!("Line Infos:");
    }
    for info in line_infos {
        println!(">>> {}", info);
    } 
*/
    Ok(())
}
